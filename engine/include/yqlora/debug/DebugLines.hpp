#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/math/Math.hpp"

#include <span>
#include <vector>

namespace yqlora {

// Вершина отладочной линии: две подряд - отрезок. Цвет - в sRGB, как его
// задали; в линейный его переводит сборка кадра.
struct DebugVertex {
    Vec3 position{0.0f};
    Vec4 color{1.0f};
};

// Чем рисуется примитив. depthTest выключен - видно сквозь стены.
struct DebugPen {
    Vec4 color{1.0f};
    bool depthTest = true;
};

// Отладочные фигуры, разобранные на отрезки: две группы, с тестом глубины
// и поверх всего. Капсула и цилиндр - вдоль своей оси Y, height - вся
// высота (у капсулы с полусферами), как у их коллайдеров.
class YQLORA_API DebugLines {
public:
    void line(const Vec3& from, const Vec3& to, const DebugPen& pen);
    // Отрезок от origin длиной и направлением direction.
    void ray(const Vec3& origin, const Vec3& direction, const DebugPen& pen);
    // Отрезок с наконечником у to.
    void arrow(const Vec3& from, const Vec3& to, const DebugPen& pen);
    // Крест из трёх отрезков по осям мира, size - его размах.
    void point(const Vec3& position, float size, const DebugPen& pen);
    void circle(const Vec3& center, const Quat& rotation, float radius, const DebugPen& pen);
    void sphere(const Vec3& center, float radius, const DebugPen& pen);
    void box(const Vec3& center, const Vec3& halfExtents, const Quat& rotation, const DebugPen& pen);
    void capsule(const Vec3& center, float radius, float height, const Quat& rotation, const DebugPen& pen);
    void cylinder(const Vec3& center, float radius, float height, const Quat& rotation, const DebugPen& pen);
    void triangle(const Vec3& a, const Vec3& b, const Vec3& c, const DebugPen& pen);
    // Оси X, Y, Z узла (красная, зелёная, синяя) длиной size от его начала.
    void axes(const Mat4& transform, float size, bool depthTest);

    void append(const DebugLines& other);
    void clear();

    bool empty() const
    {
        return m_tested.empty() && m_overlay.empty();
    }

    std::span<const DebugVertex> tested() const
    {
        return m_tested;
    }

    std::span<const DebugVertex> overlay() const
    {
        return m_overlay;
    }

private:
    // Дуга окружности радиуса radius в плоскости осей first и second вокруг
    // center, от угла from до to в радианах.
    void arc(
        const Vec3& center,
        const Vec3& first,
        const Vec3& second,
        float radius,
        float from,
        float to,
        const DebugPen& pen
    );

    std::vector<DebugVertex> m_tested;
    std::vector<DebugVertex> m_overlay;
};

} // namespace yqlora
