#pragma once

#include "yqlora/editor/ui/gizmos/Gizmo.hpp"

namespace yqlora::editor {

// Источники света в окне сцены: значок вида у каждого, а у выбранного ещё и
// то, куда он светит - лучи направленного, сфера дальности точечного, конус
// прожектора. Своей геометрии у света нет, без этого его не видно.
class LightGizmo final : public Gizmo {
public:
    bool draw(GizmoContext& context) override;
};

} // namespace yqlora::editor
