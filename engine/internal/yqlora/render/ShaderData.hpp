#pragma once

#include "yqlora/math/Math.hpp"
#include "yqlora/rhi/Pipeline.hpp"

#include <cstdint>

namespace yqlora {

// Наборы ресурсов шейдеров по частоте смены. Те же номера и имена блоков -
// в шейдерах.
inline constexpr std::uint32_t kFrameSet = 0;
inline constexpr std::uint32_t kViewSet = 1;
inline constexpr std::uint32_t kMaterialSet = 2;
inline constexpr std::uint32_t kObjectSet = 3;

// Uniform-блоки в раскладке std140: vec3 занимает 16 байт, матрицы - по
// столбцам vec4. Структуры повторяют её явно, размер проверяется ниже.
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

static_assert(sizeof(FrameUniforms) == 16);
static_assert(sizeof(ViewUniforms) == 80);
static_assert(sizeof(ObjectUniforms) == 144);

inline const rhi::ShaderBinding kFrameBlock{"Frame", rhi::BindingKind::UniformBlock, kFrameSet, 0};
inline const rhi::ShaderBinding kViewBlock{"View", rhi::BindingKind::UniformBlock, kViewSet, 0};
inline const rhi::ShaderBinding kObjectBlock{"Object", rhi::BindingKind::UniformBlock, kObjectSet, 0};

} // namespace yqlora
