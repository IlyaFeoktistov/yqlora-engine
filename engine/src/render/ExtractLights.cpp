#include "yqlora/render/FrameExtract.hpp"

#include "yqlora/core/Log.hpp"
#include "yqlora/render/RenderSystem.hpp"
#include "yqlora/render/SpotLight3D.hpp"

#include <algorithm>
#include <cmath>

namespace yqlora {

namespace {

// Вид источника - только kind(): по нему же его различает шейдер.
LightData lightData(const Light3D& light)
{
    LightData data{
        .kind = light.kind(),
        .position = light.worldPosition(),
        .direction = light.worldForward(),
        .color = math::srgbToLinear(light.color()) * light.intensity(),
    };
    switch (light.kind()) {
    case LightKind::Directional: break;
    case LightKind::Spot: {
        const auto& spot = static_cast<const SpotLight3D&>(light);
        data.cosInner = std::cos(glm::radians(spot.innerAngleDegrees()));
        data.cosOuter = std::cos(glm::radians(spot.outerAngleDegrees()));
        [[fallthrough]];
    }
    case LightKind::Point: data.range = static_cast<const PointLight3D&>(light).range(); break;
    }

    return data;
}

} // namespace

std::vector<LightData> extractLights(const RenderSystem& render, const Vec3& camera)
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
    if (trimmed && render.takeLightLimitReport()) {
        log::warn(
            "The scene has more lights than a frame takes ({} directional, {} point and spot): the nearest "
            "to the camera are kept",
            kMaxDirectionalLights,
            kMaxPunctualLights
        );
    }
    std::vector<LightData> result = std::move(directional);
    result.insert(result.end(), punctual.begin(), punctual.end());

    return result;
}

} // namespace yqlora
