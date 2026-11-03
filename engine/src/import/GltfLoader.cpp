#include "yqlora/import/GltfLoader.hpp"

#include "yqlora/core/Log.hpp"
#include "yqlora/core/Utf8Path.hpp"
#include "yqlora/import/ModelData.hpp"
#include "yqlora/render/MeshData.hpp"

#include <cgltf.h>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#include <array>
#include <cstdlib>
#include <cstring>
#include <format>
#include <fstream>
#include <iterator>
#include <memory>
#include <vector>

namespace yqlora {

namespace {

constexpr std::array<std::string_view, 2> kExtensions{".gltf", ".glb"};
// Нормаль вершины, у которой все соседние треугольники вырождены.
const Vec3 kFallbackNormal{0.0f, 1.0f, 0.0f};

struct DataDeleter {
    void operator()(cgltf_data* data) const
    {
        cgltf_free(data);
    }
};

using GltfData = std::unique_ptr<cgltf_data, DataDeleter>;

std::vector<char> readBytes(const std::filesystem::path& file)
{
    std::ifstream stream(file, std::ios::binary);
    if (!stream) {
        return {};
    }

    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

// cgltf открывает внешние буферы сам через fopen, который на Windows не
// понимает путей не из ASCII. Путь он даёт в UTF-8, а читаем мы.
cgltf_result readFile(
    const cgltf_memory_options* /*memory*/,
    const cgltf_file_options* /*file*/,
    const char* path,
    cgltf_size* size,
    void** data
)
{
    const std::vector<char> bytes = readBytes(fromUtf8(path));
    if (bytes.empty()) {
        return cgltf_result_file_not_found;
    }
    void* copy = std::malloc(bytes.size());
    if (!copy) {
        return cgltf_result_out_of_memory;
    }
    std::memcpy(copy, bytes.data(), bytes.size());
    *size = bytes.size();
    *data = copy;

    return cgltf_result_success;
}

void releaseFile(const cgltf_memory_options* /*memory*/, const cgltf_file_options* /*file*/, void* data)
{
    std::free(data);
}

const char* resultText(cgltf_result result)
{
    switch (result) {
    case cgltf_result_success: return "success";
    case cgltf_result_data_too_short: return "the data is too short";
    case cgltf_result_unknown_format: return "not a glTF file";
    case cgltf_result_invalid_json: return "invalid JSON";
    case cgltf_result_invalid_gltf: return "invalid glTF";
    case cgltf_result_invalid_options: return "invalid options";
    case cgltf_result_file_not_found: return "a buffer file is not found";
    case cgltf_result_io_error: return "a read error";
    case cgltf_result_out_of_memory: return "out of memory";
    case cgltf_result_legacy_gltf: return "glTF 1.0 is not supported";
    case cgltf_result_max_enum: break;
    }

    return "unknown error";
}

const cgltf_accessor* findAttribute(const cgltf_primitive& primitive, cgltf_attribute_type type, int index = 0)
{
    for (cgltf_size i = 0; i < primitive.attributes_count; ++i) {
        const cgltf_attribute& attribute = primitive.attributes[i];
        if (attribute.type == type && attribute.index == index) {
            return attribute.data;
        }
    }

    return nullptr;
}

// Значения аксессора подряд по n компонентов. Разреженные аксессоры,
// нормализованные целые, шаг между элементами - забота cgltf.
std::vector<float> unpackFloats(const cgltf_accessor& accessor)
{
    std::vector<float> values(accessor.count * cgltf_num_components(accessor.type));
    cgltf_accessor_unpack_floats(&accessor, values.data(), values.size());

    return values;
}

// Сглаженные нормали: нормали треугольников, сложенные по общим вершинам.
// Вершины, разделённые в файле, остаются разделёнными. Нормаль треугольника
// не нормирована, поэтому большой треугольник весит больше.
void computeNormals(std::span<Vertex> vertices, std::span<const std::uint32_t> indices)
{
    for (Vertex& vertex : vertices) {
        vertex.normal = Vec3(0.0f);
    }
    for (std::size_t i = 0; i + MeshData::kTriangleCorners <= indices.size(); i += MeshData::kTriangleCorners) {
        Vertex& a = vertices[indices[i]];
        Vertex& b = vertices[indices[i + 1]];
        Vertex& c = vertices[indices[i + 2]];
        const Vec3 face = glm::cross(b.position - a.position, c.position - a.position);
        a.normal += face;
        b.normal += face;
        c.normal += face;
    }
    for (Vertex& vertex : vertices) {
        const float length = glm::length(vertex.normal);
        vertex.normal = length > 0.0f ? vertex.normal / length : kFallbackNormal;
    }
}

// Добавляет примитив подмешем. Текст ошибки, если данные не годятся.
std::string addPrimitive(const cgltf_data& data, const cgltf_primitive& primitive, float scale, MeshData& mesh)
{
    const cgltf_accessor* positions = findAttribute(primitive, cgltf_attribute_type_position);
    if (!positions || positions->type != cgltf_type_vec3) {
        return "a primitive has no positions";
    }
    const std::size_t count = positions->count;
    const cgltf_accessor* normals = findAttribute(primitive, cgltf_attribute_type_normal);
    const cgltf_accessor* uvs = findAttribute(primitive, cgltf_attribute_type_texcoord);
    const cgltf_accessor* tangents = findAttribute(primitive, cgltf_attribute_type_tangent);
    if ((normals && (normals->count != count || normals->type != cgltf_type_vec3))
        || (uvs && (uvs->count != count || uvs->type != cgltf_type_vec2))
        || (tangents && (tangents->count != count || tangents->type != cgltf_type_vec4))) {
        return "the attributes of a primitive differ in size";
    }

    std::vector<Vertex> vertices(count);
    const std::vector<float> position = unpackFloats(*positions);
    for (std::size_t i = 0; i < count; ++i) {
        vertices[i].position = glm::make_vec3(&position[i * Vec3::length()]) * scale;
    }
    if (normals) {
        const std::vector<float> normal = unpackFloats(*normals);
        for (std::size_t i = 0; i < count; ++i) {
            vertices[i].normal = glm::make_vec3(&normal[i * Vec3::length()]);
        }
    }
    if (uvs) {
        const std::vector<float> uv = unpackFloats(*uvs);
        for (std::size_t i = 0; i < count; ++i) {
            vertices[i].uv0 = glm::make_vec2(&uv[i * Vec2::length()]);
        }
    }
    // Без TANGENT касательные остаются по умолчанию; вычисляются они для
    // карт нормалей вместе с материалами модели.
    if (tangents) {
        const std::vector<float> tangent = unpackFloats(*tangents);
        for (std::size_t i = 0; i < count; ++i) {
            vertices[i].tangent = glm::make_vec4(&tangent[i * Vec4::length()]);
        }
    }

    std::vector<std::uint32_t> indices;
    if (primitive.indices) {
        indices.resize(primitive.indices->count);
        cgltf_accessor_unpack_indices(primitive.indices, indices.data(), sizeof(std::uint32_t), indices.size());
    } else {
        // Без индексов вершины идут тройками по порядку.
        indices.resize(count);
        for (std::size_t i = 0; i < count; ++i) {
            indices[i] = static_cast<std::uint32_t>(i);
        }
    }
    if (indices.size() % MeshData::kTriangleCorners != 0) {
        return "the index count of a primitive is not a multiple of 3";
    }
    for (const std::uint32_t index : indices) {
        if (index >= count) {
            return "a primitive has an index out of its vertices";
        }
    }
    if (!normals) {
        computeNormals(vertices, indices);
    }

    const auto base = static_cast<std::uint32_t>(mesh.vertices.size());
    SubMesh submesh;
    submesh.firstIndex = static_cast<std::uint32_t>(mesh.indices.size());
    submesh.indexCount = static_cast<std::uint32_t>(indices.size());
    if (primitive.material) {
        submesh.sourceMaterial = static_cast<int>(cgltf_material_index(&data, primitive.material));
    }
    mesh.vertices.insert(mesh.vertices.end(), vertices.begin(), vertices.end());
    for (const std::uint32_t index : indices) {
        mesh.indices.push_back(base + index);
    }
    mesh.submeshes.push_back(submesh);

    return {};
}

ModelNode readNode(const cgltf_data& data, const cgltf_node& node, float scale)
{
    ModelNode result;
    result.name = node.name ? node.name : "";
    if (node.has_matrix) {
        Vec3 skew;
        Vec4 perspective;
        glm::decompose(glm::make_mat4(node.matrix), result.scale, result.rotation, result.position, skew, perspective);
    } else {
        if (node.has_translation) {
            result.position = glm::make_vec3(node.translation);
        }
        if (node.has_rotation) {
            // В glTF кватернион - x, y, z, w; glm ждёт w первым.
            result.rotation = Quat(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]);
        }
        if (node.has_scale) {
            result.scale = glm::make_vec3(node.scale);
        }
    }
    result.position *= scale;
    if (node.mesh) {
        result.mesh = cgltf_mesh_index(&data, node.mesh);
    }
    for (cgltf_size i = 0; i < node.children_count; ++i) {
        result.children.push_back(cgltf_node_index(&data, node.children[i]));
    }

    return result;
}

} // namespace

std::span<const std::string_view> GltfLoader::extensions() const
{
    return kExtensions;
}

std::string_view GltfLoader::kind() const
{
    return ModelData::kResourceKind;
}

std::span<const std::string_view> GltfLoader::nestedKinds() const
{
    static constexpr std::string_view kNested[] = {MeshData::kResourceKind};

    return kNested;
}

LoadResult GltfLoader::load(const LoadRequest& request) const
{
    const float scale = request.import.value(kScaleSetting, kDefaultScale);
    // GLB ссылается на эти байты, пока данные cgltf живы.
    const std::vector<char> bytes = readBytes(request.file);
    if (bytes.empty()) {
        return loadError("the file is empty or not readable");
    }

    cgltf_options options{};
    options.file.read = &readFile;
    options.file.release = &releaseFile;
    cgltf_data* parsed = nullptr;
    cgltf_result result = cgltf_parse(&options, bytes.data(), bytes.size(), &parsed);
    GltfData data(parsed);
    if (result == cgltf_result_success) {
        result = cgltf_load_buffers(&options, data.get(), toUtf8(request.file).c_str());
    }
    if (result == cgltf_result_success) {
        result = cgltf_validate(data.get());
    }
    if (result != cgltf_result_success) {
        return loadError(resultText(result));
    }

    LoadResult loaded;
    ModelData model;
    SubresourceKeys keys;
    for (cgltf_size m = 0; m < data->meshes_count; ++m) {
        const cgltf_mesh& source = data->meshes[m];
        MeshData mesh;
        for (cgltf_size p = 0; p < source.primitives_count; ++p) {
            const cgltf_primitive& primitive = source.primitives[p];
            if (primitive.type != cgltf_primitive_type_triangles) {
                log::warn(
                    "glTF '{}': a primitive of mesh {} is not triangles, skipped",
                    toUtf8(request.file.filename()),
                    m
                );
                continue;
            }
            if (std::string error = addPrimitive(*data, primitive, scale, mesh); !error.empty()) {
                return loadError(std::format("mesh {}: {}", m, error));
            }
        }
        mesh.bounds = computeBounds(mesh.vertices);
        std::string key = keys.next(MeshData::kResourceKind, source.name ? source.name : "", m);
        model.meshes.push_back(key);
        loaded.resources.push_back(makeLoadedResource(std::move(key), std::move(mesh)));
    }

    for (cgltf_size n = 0; n < data->nodes_count; ++n) {
        model.nodes.push_back(readNode(*data, data->nodes[n], scale));
    }
    const cgltf_scene* scene = data->scene ? data->scene : (data->scenes_count > 0 ? data->scenes : nullptr);
    if (scene) {
        for (cgltf_size i = 0; i < scene->nodes_count; ++i) {
            model.roots.push_back(cgltf_node_index(data.get(), scene->nodes[i]));
        }
    } else {
        for (cgltf_size n = 0; n < data->nodes_count; ++n) {
            if (!data->nodes[n].parent) {
                model.roots.push_back(n);
            }
        }
    }
    loaded.resources.insert(loaded.resources.begin(), makeLoadedResource(std::string(), std::move(model)));

    return loaded;
}

} // namespace yqlora
