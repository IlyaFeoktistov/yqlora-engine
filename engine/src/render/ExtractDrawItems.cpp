#include "yqlora/render/FrameExtract.hpp"

#include "yqlora/core/EngineServices.hpp"
#include "yqlora/render/MeshRenderer.hpp"
#include "yqlora/render/Prepare.hpp"
#include "yqlora/resource/ResourceCache.hpp"
#include "yqlora/scene/Scene.hpp"

namespace yqlora {

std::vector<DrawItem> extractDrawItems(const Scene& scene)
{
    std::vector<DrawItem> items;
    ResourceCache& resources = scene.services().resources();
    for (const MeshRenderer* renderer : scene.components<MeshRenderer>()) {
        if (!renderer->activeAndEnabled()) {
            continue;
        }
        const auto* node = dynamic_cast<const Node3D*>(renderer->owner());
        const ResourceId meshId = renderer->mesh().id;
        const LoadedData<MeshData> mesh = resources.get<MeshData>(meshId);
        if (node && mesh) {
            items.push_back({
                .mesh = mesh.data,
                .meshId = meshId,
                .meshRevision = mesh.revision,
                .world = node->worldMatrix(),
                .bounds = transformBounds(mesh.data->bounds, node->worldMatrix()),
                .color = renderer->color(),
                .layer = node->layer(),
                .node = node->id(),
            });
        }
    }

    return items;
}

} // namespace yqlora
