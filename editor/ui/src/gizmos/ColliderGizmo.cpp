#include "yqlora/editor/ui/gizmos/ColliderGizmo.hpp"

#include "yqlora/editor/ui/gizmos/GizmoDrawing.hpp"
#include "yqlora/editor/ui/style/Theme.hpp"

#include "yqlora/debug/DebugLines.hpp"
#include "yqlora/editor/ColliderEditing.hpp"
#include "yqlora/editor/EditorCamera.hpp"
#include "yqlora/editor/EditorSession.hpp"
#include "yqlora/editor/EditorSettings.hpp"
#include "yqlora/editor/SceneEditing.hpp"
#include "yqlora/physics/Collider.hpp"
#include "yqlora/render/MeshData.hpp"
#include "yqlora/resource/ResourceCache.hpp"
#include "yqlora/scene/Node3D.hpp"

#include <glm/geometric.hpp>
#include <imgui.h>

#include <type_traits>
#include <variant>

namespace yqlora::editor {

namespace {

// Прозрачность коллайдеров тела, которые не на выбранном узле.
constexpr float kFaintAlpha = 0.35f;

// Фигура коллайдера отрезками в его собственных осях: те же построители,
// что у отладочной отрисовки. Меш - коробкой своих границ.
DebugLines colliderLines(const Collider& collider, ResourceCache& resources)
{
    DebugLines lines;
    const DebugPen pen;
    const Vec3 origin(0.0f);
    const Quat identity(1.0f, 0.0f, 0.0f, 0.0f);
    std::visit(
        [&](const auto& shape) {
            using Shape = std::decay_t<decltype(shape)>;
            if constexpr (std::is_same_v<Shape, BoxShape>) {
                lines.box(origin, shape.halfExtents, identity, pen);
            } else if constexpr (std::is_same_v<Shape, SphereShape>) {
                lines.sphere(origin, shape.radius, pen);
            } else if constexpr (std::is_same_v<Shape, CapsuleShape>) {
                lines.capsule(origin, shape.radius, 2.0f * (shape.halfHeight + shape.radius), identity, pen);
            } else if constexpr (std::is_same_v<Shape, CylinderShape>) {
                lines.cylinder(origin, shape.radius, 2.0f * shape.halfHeight, identity, pen);
            } else if (const MeshData* mesh = resources.find<MeshData>(shape.mesh); mesh && !mesh->vertices.empty()) {
                const Bounds& bounds = mesh->bounds;
                lines.box((bounds.min + bounds.max) * 0.5f, (bounds.max - bounds.min) * 0.5f, identity, pen);
            }
        },
        collider.shape()
    );

    return lines;
}

Collider* colliderAt(Node& node, std::size_t index)
{
    const auto& components = node.components();

    return index < components.size() ? dynamic_cast<Collider*>(components[index].get()) : nullptr;
}

} // namespace

bool ColliderGizmo::draw(GizmoContext& context)
{
    auto* node = dynamic_cast<Node3D*>(context.session.selectedNode());
    Collider* dragged = m_drag && node && node->id() == m_drag->node ? colliderAt(*node, m_drag->component) : nullptr;
    if (m_drag && !dragged) {
        // Выделение или сцена сменились посреди перетаскивания.
        m_drag.reset();
    }
    if (!node) {
        return false;
    }

    ImDrawList& list = *ImGui::GetWindowDrawList();
    const Vec4& color = context.theme.colors().colliderGizmo;
    ResourceCache& resources = context.session.resources();
    for (const Collider* collider : bodyColliders(*node)) {
        const auto* owner = dynamic_cast<const Node3D*>(collider->owner());
        if (!owner || !collider->enabled()) {
            continue;
        }
        drawDebugLines(
            list,
            context,
            colliderLines(*collider, resources),
            colliderWorldMatrix(*collider, *owner),
            toImU32(color, owner == node ? 1.0f : kFaintAlpha)
        );
    }

    return !context.camera.flying() && drawHandles(context, *node);
}

bool ColliderGizmo::drawHandles(GizmoContext& context, Node3D& node)
{
    if (m_drag) {
        Collider* collider = colliderAt(node, m_drag->component);
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            continueDrag(context, *collider);
        } else {
            const ComponentSnapshot before = m_drag->before;
            m_drag.reset();
            context.session.editing().commitComponentEdit(*collider, before);
        }
    }

    ImDrawList& list = *ImGui::GetWindowDrawList();
    const Mat4& world = node.worldMatrix();
    struct Hovered {
        std::size_t component = 0;
        ShapeHandle handle;
        float distance = 0.0f;
    };
    std::optional<Hovered> hovered;
    struct Placed {
        ImVec2 screen;
        std::size_t component = 0;
        ShapeHandle handle;
    };
    std::vector<Placed> placed;
    const auto& components = node.components();
    for (std::size_t index = 0; index < components.size(); ++index) {
        const auto* collider = dynamic_cast<const Collider*>(components[index].get());
        const auto shape = collider && collider->enabled() ? colliderHandleShape(*collider) : std::nullopt;
        if (!shape) {
            continue;
        }
        for (const ShapeHandle& handle : shapeHandles()) {
            const auto screen = worldToScreen(context, Vec3(world * Vec4(handlePosition(*shape, handle), 1.0f)));
            if (!screen) {
                continue;
            }
            placed.push_back({*screen, index, handle});
            const float distance = mouseDistance(*screen);
            if (context.mouseFree && distance <= kHandlePickPixels && (!hovered || distance < hovered->distance)) {
                hovered = Hovered{index, handle, distance};
            }
        }
    }

    const EditorColors& colors = context.theme.colors();
    for (const Placed& handle : placed) {
        const bool active = m_drag
                                ? m_drag->component == handle.component && m_drag->handle == handle.handle
                                : hovered && hovered->component == handle.component && hovered->handle == handle.handle;
        drawHandle(list, handle.screen, toImU32(active ? colors.selectionOutline : colors.colliderGizmo));
    }

    if (!m_drag && hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        startDrag(context, node, hovered->component, hovered->handle);
    }

    return m_drag.has_value() || hovered.has_value();
}

void ColliderGizmo::startDrag(GizmoContext& context, Node3D& node, std::size_t component, const ShapeHandle& handle)
{
    const Collider* collider = colliderAt(node, component);
    const auto shape = collider ? colliderHandleShape(*collider) : std::nullopt;
    if (!shape) {
        return;
    }
    const Mat4& world = node.worldMatrix();
    const Vec3 along = Mat3(world) * handleDirection(*shape, handle);
    const float axisScale = glm::length(along);
    if (axisScale <= 0.0f) {
        return;
    }
    const Vec3 origin = Vec3(world * Vec4(handlePosition(*shape, handle), 1.0f));
    const Vec3 direction = along / axisScale;
    const auto startAlong = closestOnLine(mouseRay(context), origin, direction);
    if (!startAlong) {
        return;
    }

    m_drag.emplace(
        Drag{
            .node = node.id(),
            .component = component,
            .handle = handle,
            .start = *shape,
            .lineOrigin = origin,
            .lineDirection = direction,
            .axisScale = axisScale,
            .startAlong = *startAlong,
            .before = ComponentSnapshot(context.session.registry(), *collider),
        }
    );
}

void ColliderGizmo::continueDrag(GizmoContext& context, Collider& collider)
{
    const auto along = closestOnLine(mouseRay(context), m_drag->lineOrigin, m_drag->lineDirection);
    if (!along) {
        return;
    }
    const SnapSettings& snap = context.settings.snap;
    const ImGuiIO& io = ImGui::GetIO();
    const bool grid = snap.grid || io.KeyCtrl;
    // Шаг сетки - метры мира, а размеры фигуры - в её осях под масштабом узла.
    const HandleDragOptions options{
        .symmetric = io.KeyAlt,
        .snapStep = grid ? snap.moveStep / m_drag->axisScale : 0.0f,
        .minHalfExtent = kMinColliderSize,
    };
    const float distance = (*along - m_drag->startAlong) / m_drag->axisScale;
    // Пока тянут, компонент меняется напрямую; в историю - один шаг в конце.
    applyHandleShape(collider, dragShapeHandle(m_drag->start, m_drag->handle, distance, options));
}

} // namespace yqlora::editor
