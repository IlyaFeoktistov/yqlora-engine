#include "yqlora/editor/ui/gizmos/SelectionGizmo.hpp"

#include "yqlora/editor/ui/style/Theme.hpp"

#include "yqlora/editor/EditorCamera.hpp"
#include "yqlora/editor/EditorSession.hpp"
#include "yqlora/editor/Picking.hpp"

namespace yqlora::editor {

void SelectionGizmo::prepareView(ViewData& view, const RenderContext& renderer, const GizmoContext& context)
{
    if (!m_outline) {
        m_outline = std::make_unique<OutlinePass>(renderer);
    }
    const auto selected = context.session.selection().nodes();
    m_outline->setSelection({selected.begin(), selected.end()}, context.theme.colors().selectionOutline);
    view.passes.push_back(m_outline.get());
}

bool SelectionGizmo::draw(GizmoContext& context)
{
    if (!context.mouseFree || context.camera.flying() || !ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        return false;
    }
    const ImVec2 mouse = ImGui::GetMousePos();
    const Ray ray = context.camera.screenRay(
        {mouse.x - context.origin.x, mouse.y - context.origin.y},
        {context.size.x, context.size.y}
    );
    const NodeId picked = pickNode(context.session.scene(), ray);
    Selection& selection = context.session.selection();
    if (ImGui::GetIO().KeyCtrl) {
        selection.toggle(picked);
    } else {
        selection.select(picked);
    }

    return false;
}

} // namespace yqlora::editor
