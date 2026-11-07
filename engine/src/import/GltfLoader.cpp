#include "yqlora/import/GltfLoader.hpp"

#include "yqlora/core/Log.hpp"
#include "yqlora/core/Utf8Path.hpp"
#include "yqlora/import/GltfDocument.hpp"
#include "yqlora/import/GltfMaterials.hpp"
#include "yqlora/import/GltfMeshes.hpp"
#include "yqlora/import/GltfNodes.hpp"
#include "yqlora/import/ModelData.hpp"
#include "yqlora/material/MaterialData.hpp"
#include "yqlora/render/MeshData.hpp"
#include "yqlora/render/TextureData.hpp"

#include <cgltf.h>

#include <algorithm>
#include <array>
#include <iterator>
#include <string>

namespace yqlora {

namespace {

constexpr std::array<std::string_view, 2> kExtensions{".gltf", ".glb"};

// Расширения glTF, которые движок читает. Квантованные аксессоры
// (KHR_mesh_quantization) распаковывает cgltf.
constexpr std::array<std::string_view, 2> kSupportedExtensions{
    "KHR_materials_emissive_strength",
    "KHR_mesh_quantization",
};

bool supported(std::string_view extension)
{
    return std::ranges::find(kSupportedExtensions, extension) != kSupportedExtensions.end();
}

std::string joined(const std::vector<std::string_view>& names)
{
    std::string text;
    for (const std::string_view name : names) {
        text += text.empty() ? "" : ", ";
        text += name;
    }

    return text;
}

// Текст ошибки, если файл требует то, чего движок не умеет; о прочем
// незнакомом - одна строка в логе.
std::string checkExtensions(const cgltf_data& data, const std::string& fileName)
{
    std::vector<std::string_view> required;
    for (cgltf_size i = 0; i < data.extensions_required_count; ++i) {
        if (!supported(data.extensions_required[i])) {
            required.emplace_back(data.extensions_required[i]);
        }
    }
    if (!required.empty()) {
        return "the file requires extensions that are not supported: " + joined(required);
    }
    std::vector<std::string_view> ignored;
    for (cgltf_size i = 0; i < data.extensions_used_count; ++i) {
        if (!supported(data.extensions_used[i])) {
            ignored.emplace_back(data.extensions_used[i]);
        }
    }
    if (!ignored.empty()) {
        log::warn("glTF '{}': extensions are not supported and ignored: {}", fileName, joined(ignored));
    }

    return {};
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
    static constexpr std::string_view kNested[] = {
        MeshData::kResourceKind,
        MaterialData::kResourceKind,
        TextureData::kResourceKind,
    };

    return kNested;
}

LoadResult GltfLoader::load(const LoadRequest& request) const
{
    const float scale = request.import.value(kScaleSetting, kDefaultScale);
    GltfDocument::Opened opened = GltfDocument::open(request.file);
    LoadResult loaded;
    loaded.dependencies = std::move(opened.dependencies);
    if (!opened.document) {
        loaded.error = std::move(opened.error);

        return loaded;
    }
    const cgltf_data& data = opened.document->data();
    const std::string fileName = toUtf8(request.file.filename());
    if (std::string error = checkExtensions(data, fileName); !error.empty()) {
        loaded.error = std::move(error);

        return loaded;
    }

    ModelData model;
    SubresourceKeys keys;
    for (cgltf_size m = 0; m < data.meshes_count; ++m) {
        MeshData mesh;
        if (std::string error = readGltfMesh(data, m, scale, fileName, mesh); !error.empty()) {
            loaded.error = std::move(error);

            return loaded;
        }
        const char* name = data.meshes[m].name;
        std::string key = keys.next(MeshData::kResourceKind, name ? name : "", m);
        model.meshes.push_back(key);
        loaded.resources.push_back(makeLoadedResource(std::move(key), std::move(mesh)));
    }
    for (cgltf_size n = 0; n < data.nodes_count; ++n) {
        model.nodes.push_back(readGltfNode(data, n, scale));
    }
    model.roots = gltfRoots(data);
    GltfMaterials materials = readGltfMaterials(data, request.file, keys);
    model.materials = std::move(materials.materialKeys);
    std::ranges::move(materials.resources, std::back_inserter(loaded.resources));
    std::ranges::move(materials.dependencies, std::back_inserter(loaded.dependencies));
    loaded.resources.insert(loaded.resources.begin(), makeLoadedResource(std::string(), std::move(model)));

    return loaded;
}

} // namespace yqlora
