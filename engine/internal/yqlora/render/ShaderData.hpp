#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/math/Math.hpp"
#include "yqlora/render/FrameData.hpp"
#include "yqlora/rhi/RhiTypes.hpp"
#include "yqlora/shader/ShaderProfile.hpp"
#include "yqlora/shader/UniformLayout.hpp"

#include <cstddef>

namespace yqlora {

// Uniform-блоки движка (engine/shaders/include/yqlora/uniforms.glsl) в
// раскладке std140: vec3 занимает 16 байт, матрицы - по столбцам vec4.
// Структуры повторяют её явно; блок каждого шейдера сверяется с раскладками
// ниже при загрузке.
// vec3 и float рядом занимают один vec4: так структуры света без пустот.
struct alignas(16) DirectionalLightUniforms {
    // Куда свет идёт.
    Vec3 direction{0.0f, -1.0f, 0.0f};
    float padding0 = 0.0f;
    Vec3 color{0.0f};
    float padding1 = 0.0f;
};

// Точечный и прожектор. Конус - множитель saturate(cos * angleScale +
// angleOffset), как в glTF KHR_lights_punctual: у точечного scale 0 и
// offset 1, то есть без конуса.
struct alignas(16) PunctualLightUniforms {
    Vec3 position{0.0f};
    float range = 0.0f;
    Vec3 color{0.0f};
    float angleScale = 0.0f;
    Vec3 direction{0.0f, 0.0f, -1.0f};
    float angleOffset = 1.0f;
};

struct alignas(16) FrameUniforms {
    Vec3 ambient{0.0f};
    int directionalCount = 0;
    int punctualCount = 0;
    DirectionalLightUniforms directionalLights[kMaxDirectionalLights]{};
    PunctualLightUniforms punctualLights[kMaxPunctualLights]{};
};

struct alignas(16) ViewUniforms {
    Mat4 viewProjection{1.0f};
    Vec3 cameraPosition{0.0f};
    float padding = 0.0f;
};

struct alignas(16) ObjectUniforms {
    Mat4 model{1.0f};
    // mat3 в std140 - три vec4, проще хранить mat4.
    Mat4 normalMatrix{1.0f};
};

inline const UniformLayout kFrameBlock{
    "Frame",
    {kFrameSet, 0},
    sizeof(FrameUniforms),
    {
        YQLORA_UNIFORM_MEMBER(FrameUniforms, ambient),
        YQLORA_UNIFORM_MEMBER(FrameUniforms, directionalCount),
        YQLORA_UNIFORM_MEMBER(FrameUniforms, punctualCount),
        YQLORA_UNIFORM_MEMBER(FrameUniforms, directionalLights),
        YQLORA_UNIFORM_MEMBER(FrameUniforms, punctualLights),
    },
};

inline const UniformLayout kViewBlock{
    "View",
    {kViewSet, 0},
    sizeof(ViewUniforms),
    {
        YQLORA_UNIFORM_MEMBER(ViewUniforms, viewProjection),
        YQLORA_UNIFORM_MEMBER(ViewUniforms, cameraPosition),
    },
};

inline const UniformLayout kObjectBlock{
    "Object",
    {kObjectSet, 0},
    sizeof(ObjectUniforms),
    {
        YQLORA_UNIFORM_MEMBER(ObjectUniforms, model),
        YQLORA_UNIFORM_MEMBER(ObjectUniforms, normalMatrix),
    },
};

// Формат HDR-цели, в которую рисуются виды кадра (см. OutputPass).
inline constexpr rhi::Format kHdrColorFormat = rhi::Format::Rgba16Float;

// Данные блока Frame из снимка кадра.
YQLORA_API FrameUniforms frameUniforms(const FrameData& frame);

// Все блоки движка: их сверяют все шейдеры.
inline const UniformLayout* const kEngineBlocks[] = {&kFrameBlock, &kViewBlock, &kObjectBlock};

} // namespace yqlora
