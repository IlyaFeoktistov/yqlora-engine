#pragma once

#include "yqlora/editor/ui/gizmos/Gizmo.hpp"

#include "yqlora/editor/render/OutlinePass.hpp"

#include <memory>

namespace yqlora::editor {

// Выделение в окне сцены: контур вокруг выбранного узла и выбор узла кликом.
// Клик в пустоту снимает выделение. Стоит последним: клик, который занял
// другой гизмо (манипулятор, оси), выбором не считается.
class SelectionGizmo final : public Gizmo {
public:
    void prepareView(ViewData& view, const RenderContext& renderer, const GizmoContext& context) override;
    bool draw(GizmoContext& context) override;

    void onRendererLost() override
    {
        m_outline.reset();
    }

private:
    std::unique_ptr<OutlinePass> m_outline;
};

} // namespace yqlora::editor
