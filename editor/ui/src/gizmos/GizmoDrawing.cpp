#include "yqlora/editor/ui/gizmos/GizmoDrawing.hpp"

namespace yqlora::editor {

namespace {

// Точки ближе к плоскости камеры не проецируются: деление на почти ноль.
constexpr float kMinClipW = 0.01f;

} // namespace

std::optional<ImVec2> worldToScreen(const GizmoContext& context, const Vec3& point)
{
    const Vec4 clip = context.view.projection * context.view.view * Vec4(point, 1.0f);
    if (clip.w <= kMinClipW) {
        return std::nullopt;
    }
    const Vec2 ndc = Vec2(clip) / clip.w;

    return ImVec2(
        context.origin.x + (ndc.x * 0.5f + 0.5f) * context.size.x,
        context.origin.y + (0.5f - ndc.y * 0.5f) * context.size.y
    );
}

void drawWorldLine(ImDrawList& list, const GizmoContext& context, const Vec3& a, const Vec3& b, ImU32 color)
{
    const auto screenA = worldToScreen(context, a);
    const auto screenB = worldToScreen(context, b);
    if (screenA && screenB) {
        list.AddLine(*screenA, *screenB, color, kGizmoLineThickness);
    }
}

} // namespace yqlora::editor
