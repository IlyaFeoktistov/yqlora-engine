#pragma once

#include "yqlora/math/Math.hpp"
#include "yqlora/shader/ShaderProfile.hpp"
#include "yqlora/shader/UniformLayout.hpp"

#include <cstddef>

namespace yqlora {

// Uniform-блоки движка (engine/shaders/include/yqlora/uniforms.glsl) в
// раскладке std140: vec3 занимает 16 байт, матрицы - по столбцам vec4.
// Структуры повторяют её явно; блок каждого шейдера сверяется с раскладками
// ниже при загрузке.
struct alignas(16) FrameUniforms {
    Vec3 lightDirection{0.0f, -1.0f, 0.0f};
    float padding = 0.0f;
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
    Vec4 color{1.0f};
};

inline const UniformLayout kFrameBlock{
    "Frame",
    {kFrameSet, 0},
    sizeof(FrameUniforms),
    {YQLORA_UNIFORM_MEMBER(FrameUniforms, lightDirection)},
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
        YQLORA_UNIFORM_MEMBER(ObjectUniforms, color),
    },
};

// Все блоки движка: их сверяют все шейдеры.
inline const UniformLayout* const kEngineBlocks[] = {&kFrameBlock, &kViewBlock, &kObjectBlock};

} // namespace yqlora
