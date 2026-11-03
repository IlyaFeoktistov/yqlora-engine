#include "yqlora/editor/ui/gizmos/GizmoDrawing.hpp"

#include "yqlora/editor/ViewportProjection.hpp"

namespace yqlora::editor {

std::optional<ImVec2> worldToScreen(const GizmoContext& context, const Vec3& point)
{
    const auto inViewport = projectToViewport(
        context.view.projection * context.view.view,
        point,
        Vec2(context.size.x, context.size.y)
    );
    if (!inViewport) {
        return std::nullopt;
    }

    return ImVec2(context.origin.x + inViewport->x, context.origin.y + inViewport->y);
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
