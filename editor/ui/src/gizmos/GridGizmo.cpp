#include "yqlora/editor/ui/gizmos/GridGizmo.hpp"

namespace yqlora::editor {

void GridGizmo::prepareView(ViewData& view, const RenderContext& renderer, const GizmoContext& /*context*/)
{
    if (!m_pass) {
        m_pass = std::make_unique<GridPass>(renderer);
    }
    view.passes.push_back(m_pass.get());
}

} // namespace yqlora::editor
