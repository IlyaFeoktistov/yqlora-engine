#pragma once

#include "yqlora/math/Ray.hpp"
#include "yqlora/scene/Node.hpp"

#include <functional>
#include <optional>

namespace yqlora {

class Node3D;
class Scene;

} // namespace yqlora

namespace yqlora::editor {

// Своей геометрии у камеры и источника света нет, поэтому их выбирают по
// сфере такого радиуса.
inline constexpr float kHandlePickRadius = 0.3f;

// Ближайший к началу луча узел, в который попадает луч: меш - с точностью
// до треугольника, камера и источник света - по сфере вокруг них. 0, если луч ни во что не попал.
NodeId pickNode(const Scene& scene, const Ray& ray);

// Точка поверхности меша, в которую попал луч.
struct SurfaceHit {
    NodeId node = 0;
    Vec3 point{0.0f};
    // Единичная нормаль треугольника в мире.
    Vec3 normal{0.0f};
};

// Ближайшее попадание луча в меши сцены, кроме ветки skip (её узел ставят и
// сам в себя ставить не должен). nullopt - мимо всех мешей.
std::optional<SurfaceHit> raycastSurfaces(const Scene& scene, const Ray& ray, const Node* skip = nullptr);

// Вершина меша в мире и её расстояние до точки на экране в пикселях.
struct ScreenVertex {
    Vec3 world{0.0f};
    float screenDistance = 0.0f;
};

// Какие узлы участвуют в поиске вершин.
using VertexFilter = std::function<bool(const Node3D&)>;

// Вершина мешей узлов, прошедших filter, ближайшая на экране к point
// (пиксели от левого верхнего угла окна вида размером viewportSize).
// nullopt - таких вершин перед камерой нет.
std::optional<ScreenVertex> nearestVertex(
    const Scene& scene,
    const Mat4& viewProjection,
    const Vec2& viewportSize,
    const Vec2& point,
    const VertexFilter& filter
);

// Куда поставить ресурс, отпущенный в окне вида под лучом ray: точка на
// плоскости земли (y = 0), если луч в неё смотрит не дальше
// kMaxPlacementDistance, иначе точка луча на kPlacementDistance. Привязка к
// сетке и к поверхностям - отдельно, см. editor.md.
inline constexpr float kPlacementDistance = 10.0f;
inline constexpr float kMaxPlacementDistance = 100.0f;
Vec3 placementPoint(const Ray& ray);

} // namespace yqlora::editor
