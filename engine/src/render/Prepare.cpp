#include "yqlora/render/Prepare.hpp"

#include <algorithm>
#include <utility>

namespace yqlora {

namespace {

Vec4 row(const Mat4& matrix, int index)
{
    return {matrix[0][index], matrix[1][index], matrix[2][index], matrix[3][index]};
}

} // namespace

Frustum frustumFromMatrix(const Mat4& viewProjection)
{
    const Vec4 x = row(viewProjection, 0);
    const Vec4 y = row(viewProjection, 1);
    const Vec4 z = row(viewProjection, 2);
    const Vec4 w = row(viewProjection, 3);

    // -w <= x <= w, -w <= y <= w, 0 <= z <= w.
    return {{w + x, w - x, w + y, w - y, z, w - z}};
}

bool intersects(const Frustum& frustum, const Bounds& bounds)
{
    for (const Vec4& plane : frustum.planes) {
        // Угол коробки дальше всех вдоль нормали плоскости: если и он
        // снаружи, снаружи вся коробка.
        const Vec3 farthest(
            plane.x >= 0.0f ? bounds.max.x : bounds.min.x,
            plane.y >= 0.0f ? bounds.max.y : bounds.min.y,
            plane.z >= 0.0f ? bounds.max.z : bounds.min.z
        );
        if (glm::dot(Vec3(plane), farthest) + plane.w < 0.0f) {
            return false;
        }
    }

    return true;
}

Bounds transformBounds(const Bounds& bounds, const Mat4& matrix)
{
    // Центр и полуразмеры: полуразмер новой коробки - сумма модулей столбцов
    // матрицы, умноженных на старые полуразмеры.
    const Vec3 center = (bounds.min + bounds.max) * 0.5f;
    const Vec3 extent = (bounds.max - bounds.min) * 0.5f;
    const Vec3 newCenter = Vec3(matrix * Vec4(center, 1.0f));
    const Mat3 absolute(glm::abs(Vec3(matrix[0])), glm::abs(Vec3(matrix[1])), glm::abs(Vec3(matrix[2])));
    const Vec3 newExtent = absolute * extent;

    return {newCenter - newExtent, newCenter + newExtent};
}

std::vector<const DrawItem*> prepareView(std::span<const DrawItem> items, const ViewData& view)
{
    const Frustum frustum = frustumFromMatrix(view.camera.projection * view.camera.view);
    std::vector<std::pair<float, const DrawItem*>> visible;
    for (const DrawItem& item : items) {
        if (inMask(view.mask, item.layer) && intersects(frustum, item.bounds)) {
            const Vec3 offset = (item.bounds.min + item.bounds.max) * 0.5f - view.camera.position;
            visible.emplace_back(glm::dot(offset, offset), &item);
        }
    }
    // stable: при равном расстоянии порядок кадра, картинка не мерцает.
    std::ranges::stable_sort(visible, {}, &std::pair<float, const DrawItem*>::first);

    std::vector<const DrawItem*> result;
    result.reserve(visible.size());
    for (const auto& [distance, item] : visible) {
        result.push_back(item);
    }

    return result;
}

} // namespace yqlora
