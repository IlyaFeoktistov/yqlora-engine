#include "yqlora/render/Prepare.hpp"

#include <algorithm>
#include <functional>
#include <tuple>
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

std::vector<const DrawItem*> prepareView(
    std::span<const DrawItem> items,
    std::span<const FrameMaterial> materials,
    const ViewData& view
)
{
    struct Visible {
        const DrawItem* item;
        const FrameMaterial* material;
        float distance;
    };
    const Frustum frustum = frustumFromMatrix(view.camera.projection * view.camera.view);
    std::vector<Visible> opaque;
    std::vector<Visible> transparent;
    for (const DrawItem& item : items) {
        if (!inMask(view.mask, item.layer) || !intersects(frustum, item.bounds)) {
            continue;
        }
        const FrameMaterial& material = materials[item.material];
        const Vec3 offset = (item.bounds.min + item.bounds.max) * 0.5f - view.camera.position;
        const Visible visible{&item, &material, glm::dot(offset, offset)};
        (material.data->blend == MaterialBlend::Transparent ? transparent : opaque).push_back(visible);
    }
    // Непрозрачное группами по конвейеру (шейдер, состояние) и материалу,
    // внутри группы спереди назад: тест глубины отбрасывает закрытое.
    std::ranges::stable_sort(opaque, [](const Visible& a, const Visible& b) {
        const auto key = [](const Visible& v) {
            return std::tuple(
                v.material->shaderId,
                v.material->shaderRevision,
                v.material->data->cull,
                v.item->material
            );
        };

        return key(a) != key(b) ? key(a) < key(b) : a.distance < b.distance;
    });
    // Прозрачное сзади вперёд: смешивается с тем, что за ним. stable: при
    // равном расстоянии порядок кадра, картинка не мерцает.
    std::ranges::stable_sort(transparent, std::ranges::greater{}, &Visible::distance);

    std::vector<const DrawItem*> result;
    result.reserve(opaque.size() + transparent.size());
    for (const Visible& visible : opaque) {
        result.push_back(visible.item);
    }
    for (const Visible& visible : transparent) {
        result.push_back(visible.item);
    }

    return result;
}

} // namespace yqlora
