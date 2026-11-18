#include "yqlora/editor/ui/gizmos/JointGizmo.hpp"

#include "yqlora/editor/ui/gizmos/GizmoDrawing.hpp"
#include "yqlora/editor/ui/style/Theme.hpp"

#include "yqlora/debug/DebugLines.hpp"
#include "yqlora/editor/EditorCamera.hpp"
#include "yqlora/editor/EditorSession.hpp"
#include "yqlora/editor/EditorSettings.hpp"
#include "yqlora/editor/JointEditing.hpp"
#include "yqlora/editor/Picking.hpp"
#include "yqlora/editor/SceneEditing.hpp"
#include "yqlora/editor/Snapping.hpp"
#include "yqlora/physics/Joint.hpp"
#include "yqlora/scene/Node3D.hpp"

#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/trigonometric.hpp>
#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <type_traits>
#include <variant>

namespace yqlora::editor {

namespace {

// Размеры рисунка скрепления в метрах: стрелка оси, дуги и конусы.
constexpr float kAxisLength = 0.5f;
constexpr float kArcRadius = 0.35f;
constexpr float kConeLength = 0.4f;
constexpr float kTwistRadius = 0.15f;
constexpr int kArcSegments = 32;
constexpr float kFullTurnDegrees = 360.0f;
// Конус качания шире почти прямого угла рисуется как почти прямой: тангенс
// иначе уходит в бесконечность.
constexpr float kMaxDrawnHalfAngle = 89.0f;
constexpr float kMarkSize = 0.05f;
constexpr float kFaintAlpha = 0.5f;
constexpr float kHandleRadius = 5.0f;
constexpr float kHandlePickPixels = 9.0f;

Ray mouseRay(const GizmoContext& context)
{
    const ImVec2 mouse = ImGui::GetMousePos();

    return context.camera.screenRay(
        {mouse.x - context.origin.x, mouse.y - context.origin.y},
        {context.size.x, context.size.y}
    );
}

void polyline(DebugLines& lines, const std::vector<Vec3>& points, const DebugPen& pen)
{
    for (std::size_t i = 0; i + 1 < points.size(); ++i) {
        lines.line(points[i], points[i + 1], pen);
    }
}

// Дуга пределов вокруг оси и спицы к её концам.
void limitArc(DebugLines& lines, const JointWorldFrame& frame, float from, float to, float radius, const DebugPen& pen)
{
    const std::vector<Vec3> arc = jointArc(frame, from, to, radius, kArcSegments);
    polyline(lines, arc, pen);
    lines.line(frame.point, arc.front(), pen);
    lines.line(frame.point, arc.back(), pen);
}

// Конус качания: края оси, отклонённой на полуугол к плоской оси (planeHalf)
// и к нормали (normalHalf), по кругу вокруг оси.
void swingCone(
    DebugLines& lines,
    const JointWorldFrame& frame,
    float normalHalfDegrees,
    float planeHalfDegrees,
    const DebugPen& pen
)
{
    const Vec3 plane = glm::cross(frame.axis, frame.normal);
    const float towardPlane = std::tan(glm::radians(std::min(normalHalfDegrees, kMaxDrawnHalfAngle)));
    const float towardNormal = std::tan(glm::radians(std::min(planeHalfDegrees, kMaxDrawnHalfAngle)));
    std::vector<Vec3> rim;
    for (int i = 0; i <= kArcSegments; ++i) {
        const float angle = glm::radians(kFullTurnDegrees) * static_cast<float>(i) / kArcSegments;
        const Vec3 direction = glm::normalize(
            frame.axis + plane * (towardPlane * std::cos(angle)) + frame.normal * (towardNormal * std::sin(angle))
        );
        rim.push_back(frame.point + direction * kConeLength);
    }
    polyline(lines, rim, pen);
    constexpr int kEdges = 4;
    for (int i = 0; i < kEdges; ++i) {
        lines.line(frame.point, rim[static_cast<std::size_t>(i * kArcSegments / kEdges)], pen);
    }
}

void drawLimits(DebugLines& lines, const JointWorldFrame& frame, const JointDefinition& definition, const DebugPen& pen)
{
    std::visit(
        [&](const auto& kind) {
            using Kind = std::decay_t<decltype(kind)>;
            if constexpr (requires { kind.axis; }) {
                lines.arrow(frame.point, frame.point + frame.axis * kAxisLength, pen);
            }
            if constexpr (std::is_same_v<Kind, HingeJointDefinition>) {
                if (kind.limits.enabled) {
                    limitArc(lines, frame, kind.limits.min, kind.limits.max, kArcRadius, pen);
                } else {
                    polyline(lines, jointArc(frame, 0.0f, kFullTurnDegrees, kArcRadius, kArcSegments), pen);
                }
            } else if constexpr (std::is_same_v<Kind, SliderJointDefinition>) {
                if (kind.limits.enabled) {
                    const Vec3 low = frame.point + frame.axis * kind.limits.min;
                    const Vec3 high = frame.point + frame.axis * kind.limits.max;
                    lines.line(low, high, pen);
                    lines.point(low, kMarkSize, pen);
                    lines.point(high, kMarkSize, pen);
                }
            } else if constexpr (std::is_same_v<Kind, ConeJointDefinition>) {
                swingCone(lines, frame, kind.halfConeAngle, kind.halfConeAngle, pen);
            } else if constexpr (std::is_same_v<Kind, SwingTwistJointDefinition>) {
                swingCone(lines, frame, kind.normalHalfConeAngle, kind.planeHalfConeAngle, pen);
                limitArc(lines, frame, kind.twistMin, kind.twistMax, kTwistRadius, pen);
            } else if constexpr (std::is_same_v<Kind, SixDofJointDefinition>) {
                lines.ray(frame.point, frame.axis * kAxisLength, pen);
                lines.ray(frame.point, frame.normal * kAxisLength, pen);
                lines.ray(frame.point, glm::cross(frame.axis, frame.normal) * kAxisLength, pen);
            }
        },
        definition
    );
}

// Диапазон расстояния: метки на линии от точки другого тела к своей.
void drawDistance(
    DebugLines& lines,
    const DistanceJoint& joint,
    const Vec3& own,
    const Vec3& other,
    const DebugPen& pen
)
{
    const Vec3 toOwn = own - other;
    const float length = glm::length(toOwn);
    if (length <= 0.0f) {
        return;
    }
    const Vec3 direction = toOwn / length;
    for (const float distance : {joint.minDistance(), joint.maxDistance()}) {
        if (distance >= 0.0f) {
            lines.point(other + direction * distance, kMarkSize, pen);
        }
    }
}

Joint* jointAt(Node& node, std::size_t index)
{
    const auto& components = node.components();

    return index < components.size() ? dynamic_cast<Joint*>(components[index].get()) : nullptr;
}

} // namespace

bool JointGizmo::draw(GizmoContext& context)
{
    auto* node = dynamic_cast<Node3D*>(context.session.selectedNode());
    if (m_drag && !(node && node->id() == m_drag->node && jointAt(*node, m_drag->component))) {
        m_drag.reset();
    }
    if (!node) {
        return false;
    }

    const EditorColors& colors = context.theme.colors();
    const bool playing = context.session.playing();
    ImDrawList& list = *ImGui::GetWindowDrawList();
    for (const Joint* joint : node->getComponents<Joint>()) {
        // Выключенное в Play скрепление разорвалось: после разрыва компонент выключен.
        const bool broken = playing && !joint->enabled();
        const Vec4& color = broken ? colors.errorText : colors.jointGizmo;
        const DebugPen pen{.color = color};
        const JointWorldFrame frame = jointWorldFrame(*joint, *node);

        DebugLines lines;
        drawLimits(lines, frame, joint->definition(), pen);
        drawDebugLines(list, context, lines, Mat4(1.0f), toImU32(color));

        if (const auto other = connectedPoint(*joint, context.session.scene())) {
            DebugLines link;
            link.line(frame.point, *other, pen);
            if (const auto* distance = dynamic_cast<const DistanceJoint*>(joint)) {
                drawDistance(link, *distance, frame.point, *other, pen);
            }
            drawDebugLines(list, context, link, Mat4(1.0f), toImU32(color, kFaintAlpha));
        }
    }

    return !context.camera.flying() && drawAnchorHandles(context, *node);
}

bool JointGizmo::drawAnchorHandles(GizmoContext& context, Node3D& node)
{
    if (m_drag) {
        Joint* joint = jointAt(node, m_drag->component);
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            continueDrag(context, node, *joint);
        } else {
            const ComponentSnapshot before = m_drag->before;
            m_drag.reset();
            context.session.editing().commitComponentEdit(*joint, before);
        }
    }

    const EditorColors& colors = context.theme.colors();
    const ImVec2 mouse = ImGui::GetMousePos();
    ImDrawList& list = *ImGui::GetWindowDrawList();
    std::optional<std::size_t> hovered;
    float hoveredDistance = kHandlePickPixels;
    const auto& components = node.components();
    for (std::size_t index = 0; index < components.size(); ++index) {
        const auto* joint = dynamic_cast<const Joint*>(components[index].get());
        const auto screen = joint ? worldToScreen(context, jointWorldFrame(*joint, node).point) : std::nullopt;
        if (!screen) {
            continue;
        }
        const float distance = glm::length(Vec2(screen->x - mouse.x, screen->y - mouse.y));
        if (context.mouseFree && distance <= hoveredDistance) {
            hovered = index;
            hoveredDistance = distance;
        }
        const bool active = m_drag ? m_drag->component == index : hovered == index;
        list.AddCircleFilled(*screen, kHandleRadius, toImU32(active ? colors.selectionOutline : colors.jointGizmo));
    }

    if (!m_drag && hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        const Joint& joint = *jointAt(node, *hovered);
        const Vec3 anchor = jointWorldFrame(joint, node).point;
        // Плоскость лицом к камере: точка движется за курсором по экрану.
        const Vec3 toCamera = glm::normalize(Vec3(glm::inverse(context.view.view)[3]) - anchor);
        if (const auto start = rayPlane(mouseRay(context), anchor, toCamera)) {
            m_drag.emplace(
                Drag{
                    .node = node.id(),
                    .component = *hovered,
                    .planePoint = anchor,
                    .planeNormal = toCamera,
                    .offset = anchor - *start,
                    .before = ComponentSnapshot(context.session.registry(), joint),
                }
            );
        }
    }

    return m_drag.has_value() || hovered.has_value();
}

void JointGizmo::continueDrag(GizmoContext& context, Node3D& node, Joint& joint)
{
    const auto hit = rayPlane(mouseRay(context), m_drag->planePoint, m_drag->planeNormal);
    if (!hit) {
        return;
    }
    Vec3 point = *hit + m_drag->offset;
    if (context.settings.snap.grid || ImGui::GetIO().KeyCtrl) {
        point = snapToGrid(point, context.settings.snap.moveStep);
    }
    // Пока тянут, скрепление меняется напрямую; в историю - один шаг в конце.
    joint.setAnchor(Vec3(glm::inverse(node.worldMatrix()) * Vec4(point, 1.0f)));
}

} // namespace yqlora::editor
