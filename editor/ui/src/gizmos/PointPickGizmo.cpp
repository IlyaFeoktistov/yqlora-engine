#include "yqlora/editor/ui/gizmos/PointPickGizmo.hpp"

#include "yqlora/editor/ui/gizmos/GizmoDrawing.hpp"
#include "yqlora/editor/ui/style/Theme.hpp"

#include "yqlora/editor/EditorCamera.hpp"
#include "yqlora/editor/EditorSession.hpp"
#include "yqlora/editor/EditorSettings.hpp"
#include "yqlora/editor/Snapping.hpp"
#include "yqlora/i18n/Translate.hpp"

#include <imgui.h>

namespace yqlora::editor {

namespace {

constexpr float kMarkerRadius = 5.0f;
// Подсказка у курсора: сдвиг от его острия.
constexpr ImVec2 kHintOffset(16.0f, 16.0f);

void drawMarker(ImDrawList& list, const GizmoContext& context, const Vec3& point, ImU32 color)
{
    if (const auto screen = worldToScreen(context, point)) {
        list.AddCircle(*screen, kMarkerRadius, color, 0, kGizmoLineThickness);
    }
}

} // namespace

bool PointPickGizmo::draw(GizmoContext& context)
{
    PointPick& pick = context.session.pointPick();
    ImDrawList& list = *ImGui::GetWindowDrawList();
    const ImU32 color = toImU32(context.theme.colors().selectionOutline);

    const std::vector<Vec3> guide = pick.takeGuide();
    for (std::size_t i = 0; i + 1 < guide.size(); ++i) {
        drawWorldLine(list, context, guide[i], guide[i + 1], color);
    }
    for (const Vec3& point : guide) {
        drawMarker(list, context, point, color);
    }

    if (!pick.waitingIn(context.session.scene())) {
        return false;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        pick.cancel();

        return true;
    }
    if (!context.mouseFree) {
        return true;
    }

    const ImVec2 mouse = ImGui::GetMousePos();
    const Ray ray = context.camera.screenRay(
        {mouse.x - context.origin.x, mouse.y - context.origin.y},
        {context.size.x, context.size.y}
    );
    SnapSettings snap = context.settings.snap;
    snap.grid = snap.grid || ImGui::GetIO().KeyCtrl;
    drawMarker(list, context, dropPoint(context.session.scene(), ray, snap), color);
    list.AddText(ImVec2(mouse.x + kHintOffset.x, mouse.y + kHintOffset.y), color, tr("editor.chain.pickHint").c_str());
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        pick.answer(context.session.scene(), ray, snap);
    }

    return true;
}

} // namespace yqlora::editor
