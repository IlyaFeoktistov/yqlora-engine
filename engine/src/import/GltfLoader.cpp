#include "yqlora/import/GltfLoader.hpp"

#include "yqlora/core/Utf8Path.hpp"
#include "yqlora/import/GltfDocument.hpp"
#include "yqlora/import/GltfMeshes.hpp"
#include "yqlora/import/GltfNodes.hpp"
#include "yqlora/import/ModelData.hpp"
#include "yqlora/render/MeshData.hpp"

#include <cgltf.h>

#include <array>

namespace yqlora {

namespace {

constexpr std::array<std::string_view, 2> kExtensions{".gltf", ".glb"};

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
    GltfDocument::Opened opened = GltfDocument::open(request.file);
    LoadResult loaded;
    loaded.dependencies = std::move(opened.dependencies);
    if (!opened.document) {
        loaded.error = std::move(opened.error);

        return loaded;
    }
    const cgltf_data& data = opened.document->data();
    const std::string fileName = toUtf8(request.file.filename());

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
    loaded.resources.insert(loaded.resources.begin(), makeLoadedResource(std::string(), std::move(model)));

    return loaded;
}

} // namespace yqlora
