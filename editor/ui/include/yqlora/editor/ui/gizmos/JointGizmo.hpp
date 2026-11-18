#pragma once

#include "yqlora/editor/ComponentSnapshot.hpp"
#include "yqlora/editor/ui/gizmos/Gizmo.hpp"
#include "yqlora/scene/NodeId.hpp"

#include <cstddef>
#include <optional>

namespace yqlora {
class Joint;
class Node3D;
} // namespace yqlora

namespace yqlora::editor {

// Скрепления выбранного узла: точка крепления, линия к другому телу, ось и
// пределы (дуга петли, отрезок слайдера, конусы качания, диапазон
// расстояния). Точку крепления тянут ручкой в плоскости экрана, одним шагом
// отмены. Разорванное в Play скрепление - цветом ошибки.
class JointGizmo final : public Gizmo {
public:
    bool draw(GizmoContext& context) override;

private:
    struct Drag {
        NodeId node = 0;
        // Место скрепления в списке компонентов узла.
        std::size_t component = 0;
        // Плоскость, в которой тянут: через точку при нажатии, лицом к камере.
        Vec3 planePoint{0.0f};
        Vec3 planeNormal{0.0f};
        // От точки под курсором при нажатии до точки крепления.
        Vec3 offset{0.0f};
        ComponentSnapshot before;
    };

    // Ручки точек крепления; true, если курсор над ручкой или её тянут.
    bool drawAnchorHandles(GizmoContext& context, Node3D& node);
    void continueDrag(GizmoContext& context, Node3D& node, Joint& joint);

    std::optional<Drag> m_drag;
};

} // namespace yqlora::editor
