#pragma once

#include "yqlora/math/Math.hpp"

#include <optional>

namespace yqlora::editor {

// Точка в пикселях окна вида размером viewportSize: от левого верхнего угла,
// y вниз. toClip переводит point в clip space (проекция * вид, у точки меша -
// ещё и мировая матрица). nullopt - точка за камерой или почти в её
// плоскости: там проекция теряет смысл.
std::optional<Vec2> projectToViewport(const Mat4& toClip, const Vec3& point, const Vec2& viewportSize);

} // namespace yqlora::editor
