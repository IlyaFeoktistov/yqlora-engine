#include "yqlora/render/EngineUniforms.hpp"

#include <algorithm>

namespace yqlora {

namespace {

// Конус прожектора как saturate(cos * scale + offset): 1 внутри внутреннего
// угла, 0 за внешним. Делитель не меньше этого, чтобы при равных углах край
// оставался резким, а не делился на ноль.
constexpr float kMinConeWidth = 0.001f;

PunctualLightUniforms punctual(const LightData& light)
{
    PunctualLightUniforms result{
        .position = light.position,
        .range = light.range,
        .color = light.color,
        .direction = light.direction,
    };
    if (light.kind == LightKind::Spot) {
        result.angleScale = 1.0f / std::max(light.cosInner - light.cosOuter, kMinConeWidth);
        result.angleOffset = -light.cosOuter * result.angleScale;
    }

    return result;
}

// Настройки атмосферы - на мегаметр, шейдер считает на километр.
constexpr float kKilometresPerMegametre = 1000.0f;
// Высота слоя из файла может оказаться нулём: деление на неё - бесконечность.
constexpr float kMinLayerHeight = 0.01f;

} // namespace

AtmosphereUniforms atmosphereUniforms(const Atmosphere& atmosphere)
{
    const auto perKm = [](const Vec3& perMm) {
        return glm::max(perMm, Vec3(0.0f)) / kKilometresPerMegametre;
    };
    const float mieScattering = std::max(atmosphere.mieScattering, 0.0f) / kKilometresPerMegametre;
    const float mieAbsorption = std::max(atmosphere.mieAbsorption, 0.0f) / kKilometresPerMegametre;

    return {
        .rayleighScattering = perKm(atmosphere.rayleighScattering),
        .bottomRadius = atmosphere.planetRadius,
        .mieScattering = Vec3(mieScattering),
        .topRadius = atmosphere.planetRadius + atmosphere.atmosphereHeight,
        .mieExtinction = Vec3(mieScattering + mieAbsorption),
        .mieAnisotropy = atmosphere.mieAnisotropy,
        .ozoneAbsorption = perKm(atmosphere.ozoneAbsorption),
        .rayleighDensityScale = -1.0f / std::max(atmosphere.rayleighHeight, kMinLayerHeight),
        .groundAlbedo = math::srgbToLinear(atmosphere.groundAlbedo),
        .mieDensityScale = -1.0f / std::max(atmosphere.mieHeight, kMinLayerHeight),
        .enabled = atmosphere.enabled ? 1 : 0,
        .aerialPerspective = std::max(atmosphere.aerialPerspective, 0.0f),
    };
}

FrameUniforms frameUniforms(const FrameData& frame)
{
    FrameUniforms result{.ambientIntensity = frame.environment.ambientSky ? frame.environment.ambientIntensity : 0.0f};
    for (const LightData& light : frame.lights) {
        if (light.sun) {
            result.hasSun = 1;
            result.sunDirection = light.direction;
            result.sunColor = light.color;
        }
        if (light.kind == LightKind::Directional) {
            if (static_cast<std::size_t>(result.directionalCount) < kMaxDirectionalLights) {
                result.directionalLights[result.directionalCount++] = {
                    .direction = light.direction,
                    .color = light.color,
                };
            }
        } else if (static_cast<std::size_t>(result.punctualCount) < kMaxPunctualLights) {
            result.punctualLights[result.punctualCount++] = punctual(light);
        }
    }

    return result;
}

} // namespace yqlora
