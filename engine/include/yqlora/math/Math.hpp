#pragma once

#include <glm/gtc/epsilon.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace yqlora {

// Математика движка - это glm, и она часть публичного API: движок и игры
// вызывают функции glm напрямую (glm::normalize, glm::dot...). Свои типы не
// пишем: glm проверен, совпадает с раскладкой данных в шейдерах и умеет SIMD.
// Алиасы ниже - короткие имена типов, а не прослойка для замены библиотеки.
using Vec2 = glm::vec2;
using Vec3 = glm::vec3;
using Vec4 = glm::vec4;
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

} // namespace math

} // namespace yqlora
