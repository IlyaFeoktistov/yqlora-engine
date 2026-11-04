#pragma once

#include <glm/gtc/epsilon.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <cmath>

namespace yqlora {

// Математика движка - это glm, и она часть публичного API: движок и игры
// вызывают функции glm напрямую (glm::normalize, glm::dot...). Свои типы не
// пишем: glm проверен, совпадает с раскладкой данных в шейдерах и умеет SIMD.
// Алиасы ниже - короткие имена типов, а не прослойка для замены библиотеки.
using Vec2 = glm::vec2;
using Vec3 = glm::vec3;
using Vec4 = glm::vec4;
using IVec4 = glm::ivec4;
using Quat = glm::quat;
using Mat3 = glm::mat3;
using Mat4 = glm::mat4;

// Только то, чего в glm нет в нужном движку виде.
namespace math {

// Направление вверх в мире: система координат правая, +Y вверх, взгляд вдоль -Z.
inline constexpr Vec3 kWorldUp{0.0f, 1.0f, 0.0f};

// Допуск для сравнения результатов вычислений с плавающей точкой.
inline constexpr float kEpsilon = 1e-4f;

// Перспективная проекция по соглашению движка для всех графических API:
// правая система, глубина клипа от 0 (ближняя плоскость) до 1 (дальняя).
// Под свой API её приводит бэкенд (OpenGL - в шейдере, см. rendering.md).
inline Mat4 perspective(float fovYRadians, float aspect, float nearPlane, float farPlane)
{
    return glm::perspectiveRH_ZO(fovYRadians, aspect, nearPlane, farPlane);
}

// Векторы равны с точностью до epsilon по каждой координате.
template <glm::length_t Size>
bool nearlyEqual(const glm::vec<Size, float>& a, const glm::vec<Size, float>& b, float epsilon = kEpsilon)
{
    return glm::all(glm::epsilonEqual(a, b, epsilon));
}

// Цвет из sRGB, в котором его выбирает человек, в линейный RGB, в котором
// считается свет. Точная кривая sRGB (IEC 61966-2-1), а не степень 2.2.
inline float srgbToLinear(float value)
{
    constexpr float kLinearSegmentEnd = 0.04045f;
    constexpr float kLinearSlope = 12.92f;
    constexpr float kOffset = 0.055f;
    constexpr float kGamma = 2.4f;
    if (value <= kLinearSegmentEnd) {
        return value / kLinearSlope;
    }

    return std::pow((value + kOffset) / (1.0f + kOffset), kGamma);
}

inline Vec3 srgbToLinear(const Vec3& color)
{
    return {srgbToLinear(color.r), srgbToLinear(color.g), srgbToLinear(color.b)};
}

// Альфа не цвет и не меняется.
inline Vec4 srgbToLinear(const Vec4& color)
{
    return {srgbToLinear(Vec3(color)), color.a};
}

// Яркость линейного цвета по весам Rec. 709 (они же у sRGB).
inline float luminance(const Vec3& linear)
{
    return glm::dot(linear, Vec3(0.2126f, 0.7152f, 0.0722f));
}

// Обратное к srgbToLinear: линейное значение 0..1 в sRGB для показа.
inline float linearToSrgb(float value)
{
    constexpr float kLinearSegmentEnd = 0.0031308f;
    constexpr float kLinearSlope = 12.92f;
    constexpr float kOffset = 0.055f;
    constexpr float kGamma = 2.4f;
    if (value <= kLinearSegmentEnd) {
        return value * kLinearSlope;
    }

    return (1.0f + kOffset) * std::pow(value, 1.0f / kGamma) - kOffset;
}

} // namespace math

} // namespace yqlora
