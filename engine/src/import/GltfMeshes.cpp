#include "yqlora/import/GltfMeshes.hpp"

#include "yqlora/core/Log.hpp"

#include <cgltf.h>
#include <glm/geometric.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <format>
#include <span>
#include <vector>

namespace yqlora {

namespace {

// Нормаль вершины, у которой все соседние треугольники вырождены.
const Vec3 kFallbackNormal{0.0f, 1.0f, 0.0f};

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
        submesh.sourceMaterialName = primitive.material->name ? primitive.material->name : "";
    }
    mesh.vertices.insert(mesh.vertices.end(), vertices.begin(), vertices.end());
    for (const std::uint32_t index : indices) {
        mesh.indices.push_back(base + index);
    }
    mesh.submeshes.push_back(submesh);

    return {};
}

} // namespace

std::string readGltfMesh(
    const cgltf_data& data,
    std::size_t index,
    float scale,
    std::string_view fileName,
    MeshData& mesh
)
{
    const cgltf_mesh& source = data.meshes[index];
    for (cgltf_size p = 0; p < source.primitives_count; ++p) {
        const cgltf_primitive& primitive = source.primitives[p];
        if (primitive.type != cgltf_primitive_type_triangles) {
            log::warn("glTF '{}': a primitive of mesh {} is not triangles, skipped", fileName, index);
            continue;
        }
        if (std::string error = addPrimitive(data, primitive, scale, mesh); !error.empty()) {
            return std::format("mesh {}: {}", index, error);
        }
    }
    mesh.bounds = computeBounds(mesh.vertices);

    return {};
}

} // namespace yqlora
