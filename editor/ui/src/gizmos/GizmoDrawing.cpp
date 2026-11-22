#include "yqlora/editor/ui/gizmos/GizmoDrawing.hpp"

#include "yqlora/debug/DebugLines.hpp"
#include "yqlora/editor/EditorCamera.hpp"
#include "yqlora/editor/ViewportProjection.hpp"

#include <glm/geometric.hpp>

#include <span>

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

void drawDebugLines(
    ImDrawList& list,
    const GizmoContext& context,
    const DebugLines& lines,
    const Mat4& transform,
    ImU32 color
)
{
    const auto draw = [&](std::span<const DebugVertex> vertices) {
        for (std::size_t i = 0; i + 1 < vertices.size(); i += 2) {
            drawWorldLine(
                list,
                context,
                Vec3(transform * Vec4(vertices[i].position, 1.0f)),
                Vec3(transform * Vec4(vertices[i + 1].position, 1.0f)),
                color
            );
        }
    };
    draw(lines.tested());
    draw(lines.overlay());
}

Vec2 mouseInView(const GizmoContext& context)
{
    const ImVec2 mouse = ImGui::GetMousePos();

    return {mouse.x - context.origin.x, mouse.y - context.origin.y};
}

Ray mouseRay(const GizmoContext& context)
{
    return context.camera.screenRay(mouseInView(context), {context.size.x, context.size.y});
}

float mouseDistance(const ImVec2& screen)
{
    const ImVec2 mouse = ImGui::GetMousePos();

    return glm::length(Vec2(screen.x - mouse.x, screen.y - mouse.y));
}

void drawHandle(ImDrawList& list, const ImVec2& screen, ImU32 color)
{
    list.AddCircleFilled(screen, kHandleRadius, color);
}

} // namespace yqlora::editor
