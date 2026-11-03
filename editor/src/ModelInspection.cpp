#include "yqlora/editor/ModelInspection.hpp"

#include "yqlora/editor/EditorSession.hpp"
#include "yqlora/editor/ResourceChoices.hpp"
#include "yqlora/editor/ResourceInspection.hpp"
#include "yqlora/import/GltfLoader.hpp"
#include "yqlora/import/ModelData.hpp"
#include "yqlora/render/MeshData.hpp"
#include "yqlora/resource/ResourceLoader.hpp"

namespace yqlora::editor {

std::optional<ModelInspection> inspectModel(EditorSession& session, ResourceId id)
{
    const auto& project = session.project();
    const auto path = project ? project->resources().path(id) : std::nullopt;
    ResourceCache& cache = session.resources();
    const ModelData* model = path ? cache.find<ModelData>(id) : nullptr;
    if (!model) {
        return std::nullopt;
    }
    ModelInspection inspection;
    inspection.nodes = model->nodes.size();
    const nlohmann::json import = importSettingsOf(session, id);
    inspection.import.scale = import.value(GltfLoader::kScaleSetting, GltfLoader::kDefaultScale);
    for (const ResourceId mesh : nestedResources(id, *path, cache)) {
        if (const MeshData* data = cache.find<MeshData>(mesh)) {
            inspection.meshes.push_back({
                .id = mesh,
                .label = std::string(subresourceLocalKey(project->resources().key(mesh))),
                .vertices = data->vertices.size(),
                .triangles = data->indices.size() / MeshData::kTriangleCorners,
                .submeshes = data->submeshes.size(),
            });
        }
    }

    return inspection;
}

bool applyModelImport(EditorSession& session, ResourceId id, const ModelImport& settings)
{
    nlohmann::json json = importSettingsOf(session, id);
    if (settings.scale == GltfLoader::kDefaultScale) {
        json.erase(GltfLoader::kScaleSetting);
    } else {
        json[GltfLoader::kScaleSetting] = settings.scale;
    }

    return applyImportSettings(session, id, json);
}

} // namespace yqlora::editor
