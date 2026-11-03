#pragma once

#include "yqlora/editor/ui/gizmos/Gizmo.hpp"

#include "yqlora/scene/Transform3D.hpp"

#include <optional>

namespace yqlora::editor {

// Манипуляторы выбранного узла: Q - выбор, W - перемещение,
// E - поворот, R - масштаб. Своя панель инструментов в левом верхнем углу
// окна сцены и переключатель осей: свои у объекта или мировые.
//
// Привязка: кнопка с магнитом - по сетке (Ctrl во время
// перетаскивания - на время); с зажатой V ближайшая к курсору вершина
// выбранного меша тянется к вершинам других мешей; с Ctrl+Shift узел при
// перемещении встаёт на поверхность под курсором, а кнопка рядом ещё и
// поворачивает его по нормали.
class ManipulatorGizmo final : public Gizmo {
public:
    enum class Tool {
        Select,
        Move,
        Rotate,
        Scale,
    };

    bool draw(GizmoContext& context) override;

private:
    // Возвращает true, если курсор над панелью.
    bool drawToolbar(const GizmoContext& context);
    void handleHotkeys();
    // Кнопки привязки; true, если курсор над ними.
    bool drawSnapButtons(const GizmoContext& context);
    // Возвращает true, если курсор над манипулятором или его тянут.
    bool drawManipulator(GizmoContext& context);
    // С зажатой V вместо манипулятора; true, пока он занимает мышь.
    bool drawVertexSnap(GizmoContext& context, Node3D& node);
    // Перемещение по вершинам закончилось: одна правка в истории.
    void finishVertexDrag(GizmoContext& context, Node3D& node);

    Tool m_tool = Tool::Move;
    bool m_localSpace = true;

    // Перемещение по вершинам: значения узла до него и сдвиг взятой
    // вершины от положения узла в мире.
    struct VertexDrag {
        NodeId node = 0;
        TransformValues original;
        Vec3 offset{0.0f};
    };
    std::optional<VertexDrag> m_vertexDrag;
};

} // namespace yqlora::editor
