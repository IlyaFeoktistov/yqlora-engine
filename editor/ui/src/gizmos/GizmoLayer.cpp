#include "yqlora/editor/ui/gizmos/Gizmo.hpp"

namespace yqlora::editor {

void GizmoLayer::add(std::unique_ptr<Gizmo> gizmo)
{
    m_gizmos.push_back(std::move(gizmo));
}

void GizmoLayer::prepareView(ViewData& view, const RenderContext& renderer, const GizmoContext& context) const
{
    for (const auto& gizmo : m_gizmos) {
        gizmo->prepareView(view, renderer, context);
    }
}

void GizmoLayer::onRendererLost() const
{
    for (const auto& gizmo : m_gizmos) {
        gizmo->onRendererLost();
    }
}

bool GizmoLayer::draw(GizmoContext& context)
{
    bool mouseTaken = false;
    for (const auto& gizmo : m_gizmos) {
        if (gizmo->draw(context)) {
            mouseTaken = true;
            context.mouseFree = false;
        }
    }

    return mouseTaken;
}

} // namespace yqlora::editor
