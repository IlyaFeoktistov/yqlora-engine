#pragma once

#include "yqlora/editor/ui/gizmos/Gizmo.hpp"

#include "yqlora/editor/render/GridPass.hpp"

#include <memory>

namespace yqlora::editor {

// Бесконечная сетка на плоскости Y = 0 (GridPass в виде сцены).
class GridGizmo final : public Gizmo {
public:
    void prepareView(ViewData& view, const RenderContext& renderer, const GizmoContext& context) override;

    void onRendererLost() override
    {
        m_pass.reset();
    }

private:
    std::unique_ptr<GridPass> m_pass;
};

} // namespace yqlora::editor
