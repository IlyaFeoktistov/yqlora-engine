#include "yqlora/render/FrameExtract.hpp"

#include "yqlora/core/EngineServices.hpp"
#include "yqlora/render/FrameMaterials.hpp"
#include "yqlora/render/MeshRenderer.hpp"
#include "yqlora/render/Prepare.hpp"
#include "yqlora/resource/ResourceCache.hpp"
#include "yqlora/scene/Scene.hpp"

namespace yqlora {

void extractDrawItems(const Scene& scene, FrameMaterials& materials, FrameData& frame)
{
    ResourceCache& resources = scene.services().resources();
    for (const MeshRenderer* renderer : scene.components<MeshRenderer>()) {
        if (!renderer->activeAndEnabled()) {
            continue;
        }
        const auto* node = dynamic_cast<const Node3D*>(renderer->owner());
        const ResourceId meshId = renderer->mesh().id;
        const LoadedData<MeshData> mesh = resources.get<MeshData>(meshId);
        if (!node || !mesh) {
            continue;
        }
        const Bounds bounds = transformBounds(mesh.data->bounds, node->worldMatrix());
        for (std::size_t slot = 0; slot < mesh.data->submeshes.size(); ++slot) {
            const SubMesh& submesh = mesh.data->submeshes[slot];
            DrawItem item{
                .mesh = mesh.data,
                .meshId = meshId,
                .meshRevision = mesh.revision,
                .firstIndex = submesh.firstIndex,
                .indexCount = submesh.indexCount,
                .world = node->worldMatrix(),
                .bounds = bounds,
                .material = materials.surface(renderer->material(slot).id),
                .layer = node->layer(),
                .node = node->id(),
            };
            if (const MaterialOverrides* overrides = renderer->findOverrides(slot)) {
                item.overrides = materials.addOverrides(item.material, *overrides);
            }
            frame.drawItems.push_back(item);
        }
    }
}

} // namespace yqlora
