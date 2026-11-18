#pragma once

#include "yqlora/editor/ui/gizmos/Gizmo.hpp"

namespace yqlora::editor {

// Выбор точки щелчком в окне сцены для инструментов (PointPick): метка
// под курсором там, куда встанет точка, щелчок отвечает, Escape отменяет.
// Ещё рисует линию-подсказку открытого инструмента (путь цепи).
class PointPickGizmo final : public Gizmo {
public:
    bool draw(GizmoContext& context) override;
};

} // namespace yqlora::editor
