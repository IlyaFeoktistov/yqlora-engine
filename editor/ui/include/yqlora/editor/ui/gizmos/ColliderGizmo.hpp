#pragma once

#include "yqlora/editor/ComponentSnapshot.hpp"
#include "yqlora/editor/ShapeHandles.hpp"
#include "yqlora/editor/ui/gizmos/Gizmo.hpp"
#include "yqlora/scene/NodeId.hpp"

#include <cstddef>
#include <optional>

namespace yqlora {
class Collider;
class Node3D;
} // namespace yqlora

namespace yqlora::editor {

// Коллайдеры выделения каркасом (коллайдеры выбранного узла ярко, другие
// коллайдеры его тела бледно) и ручки на сторонах их фигур: перетаскивание
// меняет размер и центр, с Alt - обе стороны, с привязкой - по шагу сетки.
// Устройство - docs/architecture/physics.md, раздел "Editor".
class ColliderGizmo final : public Gizmo {
public:
    bool draw(GizmoContext& context) override;

private:
    struct Drag {
        NodeId node = 0;
        // Место коллайдера в списке компонентов узла.
        std::size_t component = 0;
        ShapeHandle handle;
        HandleShape start;
        // Прямая ручки в мире: откуда и куда, единичный вектор.
        Vec3 lineOrigin{0.0f};
        Vec3 lineDirection{0.0f};
        // Метров мира на единицу фигуры вдоль ручки (масштаб узла).
        float axisScale = 1.0f;
        // Где на прямой был курсор при нажатии.
        float startAlong = 0.0f;
        ComponentSnapshot before;
    };

    // Ручки коллайдеров узла; true, если курсор над ручкой или её тянут.
    bool drawHandles(GizmoContext& context, Node3D& node);
    void startDrag(GizmoContext& context, Node3D& node, std::size_t component, const ShapeHandle& handle);
    void continueDrag(GizmoContext& context, Collider& collider);

    std::optional<Drag> m_drag;
};

} // namespace yqlora::editor
