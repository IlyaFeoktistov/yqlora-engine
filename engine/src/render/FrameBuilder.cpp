#include "yqlora/render/FrameBuilder.hpp"

#include "yqlora/core/EngineServices.hpp"
#include "yqlora/core/Log.hpp"
#include "yqlora/render/Camera3D.hpp"
#include "yqlora/render/LightingSettings.hpp"
#include "yqlora/render/MeshData.hpp"
#include "yqlora/render/MeshRenderer.hpp"
#include "yqlora/render/Prepare.hpp"
#include "yqlora/render/RenderSystem.hpp"
#include "yqlora/render/SpotLight3D.hpp"
#include "yqlora/resource/ResourceCache.hpp"
#include "yqlora/scene/Scene.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace yqlora {

namespace {

LightData lightData(const Light3D& light)
{
    LightData data{
        .kind = light.kind(),
        .position = light.worldPosition(),
        .direction = light.worldForward(),
        .color = math::srgbToLinear(light.color()) * light.intensity(),
    };
    if (const auto* point = dynamic_cast<const PointLight3D*>(&light)) {
        data.range = point->range();
    }
    if (const auto* spot = dynamic_cast<const SpotLight3D*>(&light)) {
        data.cosInner = std::cos(glm::radians(spot->innerAngleDegrees()));
        data.cosOuter = std::cos(glm::radians(spot->outerAngleDegrees()));
    }

    return data;
}

// Источники сцены в пределах блока кадра: направленные - первые по
// порядку, точечные и прожекторы - ближайшие к камере. false, если
// пришлось отбросить.
bool collectLights(const RenderSystem& render, const Vec3& camera, std::vector<LightData>& result)
{
    std::vector<LightData> directional;
    std::vector<LightData> punctual;
    for (const Light3D* light : render.lights()) {
        (light->kind() == LightKind::Directional ? directional : punctual).push_back(lightData(*light));
    }
    const bool trimmed = directional.size() > kMaxDirectionalLights || punctual.size() > kMaxPunctualLights;
    if (punctual.size() > kMaxPunctualLights) {
        const auto distance = [&](const LightData& light) {
            const Vec3 offset = light.position - camera;

            return glm::dot(offset, offset);
        };
        // stable: при равном расстоянии порядок сцены, свет не мигает.
        std::ranges::stable_sort(punctual, {}, distance);
        punctual.resize(kMaxPunctualLights);
    }
    directional.resize(std::min(directional.size(), kMaxDirectionalLights));
    result = std::move(directional);
    result.insert(result.end(), punctual.begin(), punctual.end());

    return !trimmed;
}

} // namespace

FrameData extractFrame(const Scene& scene, float aspect)
{
    FrameData frame;
    frame.ambient = lightingSettingsOf(scene.services()).linearAmbient();
    if (const RenderSystem* render = scene.system<RenderSystem>()) {
        for (const Camera3D* camera : render->cameras()) {
            frame.views.push_back(cameraView(*camera, aspect));
        }
        const Vec3 camera = frame.views.empty() ? Vec3(0.0f) : frame.views.front().camera.position;
        if (!collectLights(*render, camera, frame.lights) && render->takeLightLimitReport()) {
            log::warn(
                "The scene has more lights than a frame takes ({} directional, {} point and spot): the nearest "
                "to the camera are kept",
                kMaxDirectionalLights,
                kMaxPunctualLights
            );
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
        .background = math::srgbToLinear(camera.background()),
        .mask = camera.cullingMask(),
    };
}

} // namespace yqlora
