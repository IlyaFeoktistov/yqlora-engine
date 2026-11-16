#include "yqlora/debug/DebugLines.hpp"

#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <array>
#include <cmath>

namespace yqlora {

namespace {

// Отрезков на полную окружность: круг шара в полэкрана ещё круглый.
constexpr int kCircleSegments = 32;

// Наконечник стрелки - доля её длины, ширина - доля длины наконечника.
constexpr float kArrowHeadShare = 0.2f;
constexpr float kArrowHeadWidth = 0.4f;

const Vec4 kAxisX(1.0f, 0.0f, 0.0f, 1.0f);
const Vec4 kAxisY(0.0f, 1.0f, 0.0f, 1.0f);
const Vec4 kAxisZ(0.0f, 0.0f, 1.0f, 1.0f);

const Vec3 kRight(1.0f, 0.0f, 0.0f);
const Vec3 kUp(0.0f, 1.0f, 0.0f);
const Vec3 kForward(0.0f, 0.0f, 1.0f);

// Две единичные оси, перпендикулярные единичной axis и друг другу.
std::array<Vec3, 2> perpendiculars(const Vec3& axis)
{
    const Vec3 helper = std::abs(axis.y) < 1.0f - math::kEpsilon ? kUp : kRight;
    const Vec3 first = glm::normalize(glm::cross(axis, helper));

    return {first, glm::cross(axis, first)};
}

} // namespace

void DebugLines::line(const Vec3& from, const Vec3& to, const DebugPen& pen)
{
    std::vector<DebugVertex>& target = pen.depthTest ? m_tested : m_overlay;
    target.push_back({from, pen.color});
    target.push_back({to, pen.color});
}

void DebugLines::ray(const Vec3& origin, const Vec3& direction, const DebugPen& pen)
{
    line(origin, origin + direction, pen);
}

void DebugLines::arrow(const Vec3& from, const Vec3& to, const DebugPen& pen)
{
    line(from, to, pen);
    const Vec3 shaft = to - from;
    const float length = glm::length(shaft);
    if (length < math::kEpsilon) {
        return;
    }
    const Vec3 direction = shaft / length;
    const float head = length * kArrowHeadShare;
    const Vec3 base = to - direction * head;
    for (const Vec3& side : perpendiculars(direction)) {
        line(to, base + side * head * kArrowHeadWidth, pen);
        line(to, base - side * head * kArrowHeadWidth, pen);
    }
}

void DebugLines::point(const Vec3& position, float size, const DebugPen& pen)
{
    const float half = size * 0.5f;
    for (const Vec3& axis : {kRight, kUp, kForward}) {
        line(position - axis * half, position + axis * half, pen);
    }
}

void DebugLines::arc(
    const Vec3& center,
    const Vec3& first,
    const Vec3& second,
    float radius,
    float from,
    float to,
    const DebugPen& pen
)
{
    const float share = std::abs(to - from) / glm::two_pi<float>();
    const int segments = std::max(1, static_cast<int>(std::ceil(share * kCircleSegments)));
    const auto at = [&](float angle) {
        return center + (first * std::cos(angle) + second * std::sin(angle)) * radius;
    };
    Vec3 previous = at(from);
    for (int i = 1; i <= segments; ++i) {
        const Vec3 next = at(from + (to - from) * static_cast<float>(i) / static_cast<float>(segments));
        line(previous, next, pen);
        previous = next;
    }
}

void DebugLines::circle(const Vec3& center, const Quat& rotation, float radius, const DebugPen& pen)
{
    // Окружность в плоскости XZ своих осей: вокруг оси Y.
    arc(center, rotation * kRight, rotation * kForward, radius, 0.0f, glm::two_pi<float>(), pen);
}

void DebugLines::sphere(const Vec3& center, float radius, const DebugPen& pen)
{
    const float full = glm::two_pi<float>();
    arc(center, kRight, kForward, radius, 0.0f, full, pen);
    arc(center, kRight, kUp, radius, 0.0f, full, pen);
    arc(center, kUp, kForward, radius, 0.0f, full, pen);
}

void DebugLines::box(const Vec3& center, const Vec3& halfExtents, const Quat& rotation, const DebugPen& pen)
{
    const auto corner = [&](int index) {
        const Vec3 sign(index & 1 ? 1.0f : -1.0f, index & 2 ? 1.0f : -1.0f, index & 4 ? 1.0f : -1.0f);

        return center + rotation * (sign * halfExtents);
    };
    // Ребро соединяет углы, номера которых отличаются одним битом.
    for (int index = 0; index < 8; ++index) {
        for (const int bit : {1, 2, 4}) {
            if ((index & bit) == 0) {
                line(corner(index), corner(index | bit), pen);
            }
        }
    }
}

void DebugLines::capsule(const Vec3& center, float radius, float height, const Quat& rotation, const DebugPen& pen)
{
    const float halfLine = std::max(height * 0.5f - radius, 0.0f);
    const Vec3 up = rotation * kUp;
    const Vec3 right = rotation * kRight;
    const Vec3 forward = rotation * kForward;
    const Vec3 top = center + up * halfLine;
    const Vec3 bottom = center - up * halfLine;
    circle(top, rotation, radius, pen);
    circle(bottom, rotation, radius, pen);
    for (const Vec3& side : {right, -right, forward, -forward}) {
        line(top + side * radius, bottom + side * radius, pen);
    }
    const float pi = glm::pi<float>();
    for (const Vec3& side : {right, forward}) {
        arc(top, side, up, radius, 0.0f, pi, pen);
        arc(bottom, side, up, radius, pi, 2.0f * pi, pen);
    }
}

void DebugLines::cylinder(const Vec3& center, float radius, float height, const Quat& rotation, const DebugPen& pen)
{
    const Vec3 up = rotation * kUp * (height * 0.5f);
    const Vec3 right = rotation * kRight;
    const Vec3 forward = rotation * kForward;
    circle(center + up, rotation, radius, pen);
    circle(center - up, rotation, radius, pen);
    for (const Vec3& side : {right, -right, forward, -forward}) {
        line(center + up + side * radius, center - up + side * radius, pen);
    }
}

void DebugLines::triangle(const Vec3& a, const Vec3& b, const Vec3& c, const DebugPen& pen)
{
    line(a, b, pen);
    line(b, c, pen);
    line(c, a, pen);
}

void DebugLines::axes(const Mat4& transform, float size, bool depthTest)
{
    const Vec3 origin(transform[3]);
    const std::array<Vec4, 3> colors{kAxisX, kAxisY, kAxisZ};
    for (int axis = 0; axis < 3; ++axis) {
        const Vec3 column(transform[axis]);
        const float length = glm::length(column);
        if (length < math::kEpsilon) {
            continue;
        }
        arrow(origin, origin + column / length * size, {.color = colors[axis], .depthTest = depthTest});
    }
}

void DebugLines::append(const DebugLines& other)
{
    m_tested.insert(m_tested.end(), other.m_tested.begin(), other.m_tested.end());
    m_overlay.insert(m_overlay.end(), other.m_overlay.begin(), other.m_overlay.end());
}

void DebugLines::clear()
{
    m_tested.clear();
    m_overlay.clear();
}

} // namespace yqlora
