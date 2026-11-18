#pragma once

#include "yqlora/math/Math.hpp"
#include "yqlora/math/Ray.hpp"

#include <array>
#include <cstdint>
#include <optional>

namespace yqlora::editor {

// Какие оси фигуры всегда одного размера: у сферы все три (радиус), у
// капсулы и цилиндра X и Z (радиус поперёк оси Y), у коробки ни одна.
enum class ShapeLinks : std::uint8_t {
    None,
    All,
    Radial,
};

// Фигура, которую правят ручками на сторонах: коробка половинами сторон в
// своих осях. center и rotation - место и поворот фигуры в осях узла.
// Коллайдеры и фигуры блокаута переводят себя в неё и обратно.
struct HandleShape {
    Vec3 center{0.0f};
    Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    Vec3 halfExtents{0.5f};
    ShapeLinks links = ShapeLinks::None;

    bool operator==(const HandleShape&) const = default;
};

// Ручка на стороне фигуры: ось фигуры (0 - X, 1 - Y, 2 - Z) и сторона
// (+1 или -1).
struct ShapeHandle {
    int axis = 0;
    float side = 1.0f;

    bool operator==(const ShapeHandle&) const = default;
};

inline constexpr int kShapeHandleCount = 6;

// Ручки на всех шести сторонах: у каждой фигуры по ручке на сторону оси.
std::array<ShapeHandle, kShapeHandleCount> shapeHandles();

// Где ручка в осях узла: середина стороны.
Vec3 handlePosition(const HandleShape& shape, const ShapeHandle& handle);

// Куда ручка смотрит наружу, единичный вектор в осях узла.
Vec3 handleDirection(const HandleShape& shape, const ShapeHandle& handle);

struct HandleDragOptions {
    // Обе стороны оси двигаются, центр стоит.
    bool symmetric = false;
    // Размер вдоль оси кратен шагу; 0 - без привязки.
    float snapStep = 0.0f;
    // Меньше этой половины стороны фигура не становится.
    float minHalfExtent = 0.0f;
};

// Фигура после того, как ручку handle утянули наружу на distance (в осях
// фигуры; отрицательное - внутрь) от фигуры start. Без symmetric
// противоположная сторона стоит, поэтому центр сдвигается на половину
// изменения размера. Связанные оси получают тот же размер.
HandleShape dragShapeHandle(
    const HandleShape& start,
    const ShapeHandle& handle,
    float distance,
    const HandleDragOptions& options
);

// Где на прямой origin + t * direction (direction единичный) ближе всего
// проходит луч ray: t. nullopt, если луч почти параллелен прямой и точка
// не определена.
std::optional<float> closestOnLine(const Ray& ray, const Vec3& origin, const Vec3& direction);

} // namespace yqlora::editor
