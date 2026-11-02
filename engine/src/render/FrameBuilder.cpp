#include "yqlora/render/FrameBuilder.hpp"

#include "yqlora/core/EngineServices.hpp"
#include "yqlora/render/Camera3D.hpp"
#include "yqlora/render/MeshData.hpp"
#include "yqlora/render/MeshRenderer.hpp"
#include "yqlora/render/Prepare.hpp"
#include "yqlora/render/RenderSystem.hpp"
#include "yqlora/resource/ResourceCache.hpp"
#include "yqlora/scene/Scene.hpp"

namespace yqlora {

FrameData extractFrame(const Scene& scene, float aspect)
{
    FrameData frame;
    if (const RenderSystem* render = scene.system<RenderSystem>()) {
        for (const Camera3D* camera : render->cameras()) {
            frame.views.push_back(cameraView(*camera, aspect));
        }
    }
    ResourceCache& resources = scene.services().resources();
    for (const MeshRenderer* renderer : scene.components<MeshRenderer>()) {
        if (!renderer->activeAndEnabled()) {
            continue;
        }
        const auto* node = dynamic_cast<const Node3D*>(renderer->owner());
        const ResourceId meshId = renderer->mesh().id;
        const LoadedData<MeshData> mesh = resources.get<MeshData>(meshId);
        if (node && mesh) {
            frame.drawItems.push_back({
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

    return frame;
}

ViewData cameraView(const Camera3D& camera, float aspect)
{
    return {
        .camera =
            {
                .view = camera.viewMatrix(),
                .projection = camera.projectionMatrix(aspect),
                .position = camera.worldPosition(),
            },
        .clear = camera.clear(),
        .background = camera.background(),
        .mask = camera.cullingMask(),
    };
}

} // namespace yqlora
