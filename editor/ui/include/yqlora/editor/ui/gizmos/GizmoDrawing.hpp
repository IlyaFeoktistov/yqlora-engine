#pragma once

#include "yqlora/editor/ui/gizmos/Gizmo.hpp"

#include "yqlora/math/Ray.hpp"

#include <imgui.h>

namespace yqlora {
class DebugLines;
} // namespace yqlora

#include <optional>

namespace yqlora::editor {

// Толщина линий контуров (камеры, свет) в пикселях.
inline constexpr float kGizmoLineThickness = 1.5f;
// Ручка, которую тянут мышью (стороны коллайдера, точка скрепления): радиус
// кружка и как близко к ней должен быть курсор, чтобы её поймать, в пикселях.
inline constexpr float kHandleRadius = 5.0f;
inline constexpr float kHandlePickPixels = 9.0f;

// Курсор в пикселях от угла картинки сцены.
Vec2 mouseInView(const GizmoContext& context);
// Луч из камеры через курсор.
Ray mouseRay(const GizmoContext& context);
// Расстояние от курсора до точки экрана, в пикселях.
float mouseDistance(const ImVec2& screen);
// Ручка-кружок в точке экрана screen.
void drawHandle(ImDrawList& list, const ImVec2& screen, ImU32 color);

// Точка мира на экране окна сцены. nullopt, если она за камерой: деление на
// почти ноль, такую точку не рисуют.
std::optional<ImVec2> worldToScreen(const GizmoContext& context, const Vec3& point);

// Отрезок мира, если оба конца перед камерой.
void drawWorldLine(ImDrawList& list, const GizmoContext& context, const Vec3& a, const Vec3& b, ImU32 color);

// Отрезки lines (оба набора, с тестом глубины и без), переведённые в мир
// матрицей transform, одним цветом: фигуры гизмо строятся теми же
// построителями, что и отладочная отрисовка.
void drawDebugLines(
    ImDrawList& list,
    const GizmoContext& context,
    const DebugLines& lines,
    const Mat4& transform,
    ImU32 color
);

} // namespace yqlora::editor
