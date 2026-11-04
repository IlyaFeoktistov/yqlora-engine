#include "yqlora/render/EngineUniforms.hpp"

#include "yqlora/render/FrameExtract.hpp"

#include <algorithm>
#include <cmath>

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
            result.sunAngularRadius = light.sourceSize;
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

ShadowUniforms shadowUniforms(const FrameData& frame, const ViewData& view, std::span<const ShadowCascade> cascades)
{
    const ShadowSettings& settings = frame.shadows;
    const CameraData& camera = view.camera;
    ShadowUniforms result{
        .cameraForward = -Vec3(camera.view[0][2], camera.view[1][2], camera.view[2][2]),
        .cascadeCount = static_cast<int>(cascades.size()),
        .blockerSamples = settings.blockerSamples,
        .filterSamples = settings.filterSamples,
        .maxPenumbra = settings.maxPenumbra,
        .cascadeSize = static_cast<float>(settings.cascadeSize),
        .tileSize = static_cast<float>(settings.atlasSize / kShadowAtlasColumns),
        .showCascades = view.showShadowCascades ? 1 : 0,
    };
    for (std::size_t i = 0; i < cascades.size() && i < kMaxShadowCascades; ++i) {
        result.cascadeMatrices[i] = cascades[i].viewProjection;
        result.cascadeSplits[static_cast<int>(i)] = cascades[i].splitFar;
        result.cascadeWidths[static_cast<int>(i)] = cascades[i].width;
        result.cascadeDepthRanges[static_cast<int>(i)] = cascades[i].depthRange;
    }
    for (std::size_t i = 0; i < frame.shadowTiles.size() && i < kMaxShadowTiles; ++i) {
        result.tileMatrices[i] = frame.shadowTiles[i].viewProjection;
    }
    std::size_t punctualIndex = 0;
    for (const LightData& light : frame.lights) {
        if (light.kind == LightKind::Directional) {
            if (light.sun && !cascades.empty()) {
                result.sunShadows = static_cast<int>(light.shadows);
                result.sunShadowStrength = light.shadowStrength;
                result.sunShadowSoftness = light.shadowSoftness;
                result.sunShadowBias = light.shadowBias;
                result.sunShadowNormalBias = light.shadowNormalBias;
                result.sunSourceTangent = std::tan(light.sourceSize);
            }
            continue;
        }
        if (punctualIndex >= kMaxPunctualLights) {
            break;
        }
        const bool shadowed = light.firstShadowTile >= 0 && light.shadows != ShadowMode::None;
        result.punctualShadows[punctualIndex] = IVec4(
            shadowed ? light.firstShadowTile : -1,
            shadowed ? static_cast<int>(light.shadows) : 0,
            0,
            0
        );
        result.punctualShadowParams[punctualIndex] = Vec4(
            light.shadowStrength,
            light.shadowSoftness,
            light.shadowBias,
            light.shadowNormalBias
        );
        result.punctualShadowSizes[punctualIndex] = Vec4(
            light.sourceSize,
            1.0f / std::tan(shadowTileHalfFov(light)),
            shadowTileFar(light),
            0.0f
        );
        ++punctualIndex;
    }

    return result;
}

} // namespace yqlora
