#pragma once

#include "yqlora/editor/ui/gizmos/Gizmo.hpp"

#include <imgui.h>

#include <optional>

namespace yqlora::editor {

// Толщина линий контуров (камеры, свет) в пикселях.
inline constexpr float kGizmoLineThickness = 1.5f;

// Точка мира на экране окна сцены. nullopt, если она за камерой: деление на
// почти ноль, такую точку не рисуют.
std::optional<ImVec2> worldToScreen(const GizmoContext& context, const Vec3& point);

// Отрезок мира, если оба конца перед камерой.
void drawWorldLine(ImDrawList& list, const GizmoContext& context, const Vec3& a, const Vec3& b, ImU32 color);

} // namespace yqlora::editor
