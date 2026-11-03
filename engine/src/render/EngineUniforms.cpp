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

} // namespace

FrameUniforms frameUniforms(const FrameData& frame)
{
    FrameUniforms result{.ambient = frame.environment.ambient};
    for (const LightData& light : frame.lights) {
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
