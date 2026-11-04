#include "yqlora/render/FrameExtract.hpp"

#include "yqlora/core/Log.hpp"
#include "yqlora/render/DirectionalLight3D.hpp"
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
        .shadows = light.shadows(),
        .shadowStrength = light.shadowStrength(),
        .shadowSoftness = light.shadowSoftness(),
        .shadowBias = light.shadowBias(),
        .shadowNormalBias = light.shadowNormalBias(),
    };
    switch (light.kind()) {
    case LightKind::Directional:
        data.sourceSize = glm::radians(static_cast<const DirectionalLight3D&>(light).angularDiameter()) * 0.5f;
        break;
    case LightKind::Spot: {
        const auto& spot = static_cast<const SpotLight3D&>(light);
        data.cosInner = std::cos(glm::radians(spot.innerAngleDegrees()));
        data.cosOuter = std::cos(glm::radians(spot.outerAngleDegrees()));
        [[fallthrough]];
    }
    case LightKind::Point: {
        const auto& point = static_cast<const PointLight3D&>(light);
        data.range = point.range();
        data.sourceSize = point.sourceRadius();
        break;
    }
    }

    return data;
}

// Солнце - первый с флажком sun, без флажков - самый яркий; оно встаёт
// первым среди направленных, чтобы его не отбросил предел кадра.
void markSun(std::vector<LightData>& directional, const std::vector<const DirectionalLight3D*>& nodes)
{
    if (directional.empty()) {
        return;
    }
    auto flagged = std::ranges::find_if(nodes, &DirectionalLight3D::sun);
    std::size_t sun = 0;
    if (flagged != nodes.end()) {
        sun = static_cast<std::size_t>(flagged - nodes.begin());
    } else {
        const auto brightest = std::ranges::max_element(directional, {}, [](const LightData& light) {
            return math::luminance(light.color);
        });
        sun = static_cast<std::size_t>(brightest - directional.begin());
    }
    directional[sun].sun = true;
    std::rotate(
        directional.begin(),
        directional.begin() + static_cast<std::ptrdiff_t>(sun),
        directional.begin() + static_cast<std::ptrdiff_t>(sun) + 1
    );
}

} // namespace

std::vector<LightData> extractLights(const RenderSystem& render, const Vec3& camera)
{
    std::vector<LightData> directional;
    std::vector<const DirectionalLight3D*> directionalNodes;
    std::vector<LightData> punctual;
    for (const Light3D* light : render.lights()) {
        if (light->kind() == LightKind::Directional) {
            directional.push_back(lightData(*light));
            directionalNodes.push_back(static_cast<const DirectionalLight3D*>(light));
        } else {
            punctual.push_back(lightData(*light));
        }
    }
    markSun(directional, directionalNodes);
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
    if (trimmed && render.takeReport(RenderReport::LightLimit)) {
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
