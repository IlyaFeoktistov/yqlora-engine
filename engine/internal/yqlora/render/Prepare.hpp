#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/render/FrameData.hpp"
#include "yqlora/render/MeshData.hpp"

#include <array>
#include <span>
#include <vector>

namespace yqlora {

// Пирамида видимости: шесть плоскостей (a, b, c, d), внутри - a*x + b*y +
// c*z + d >= 0. Из матрицы вида и проекции по соглашению движка (глубина
// клипа 0..1).
struct Frustum {
    std::array<Vec4, 6> planes{};
};

YQLORA_API Frustum frustumFromMatrix(const Mat4& viewProjection);

// Пересекает ли коробка пирамиду или лежит в ней. Консервативно: коробку у
// угла пирамиды может признать видимой, невидимую - никогда.
YQLORA_API bool intersects(const Frustum& frustum, const Bounds& bounds);

// Коробка по осям вокруг bounds, преобразованной matrix.
YQLORA_API Bounds transformBounds(const Bounds& bounds, const Mat4& matrix);

// Prepare одного вида: что из кадра вид рисует и в каком порядке. Слой в
// маске вида, границы в пирамиде его камеры. Сначала непрозрачное, группами
// по конвейеру и материалу, внутри группы спереди назад; потом прозрачное
// сзади вперёд. materials - FrameData::materials.
YQLORA_API std::vector<const DrawItem*> prepareView(
    std::span<const DrawItem> items,
    std::span<const FrameMaterial> materials,
    const ViewData& view
);

} // namespace yqlora
