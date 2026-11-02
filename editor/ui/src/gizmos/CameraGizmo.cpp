#include "yqlora/editor/ui/gizmos/CameraGizmo.hpp"

#include "yqlora/editor/ui/gizmos/GizmoDrawing.hpp"
#include "yqlora/editor/ui/style/Theme.hpp"

#include "yqlora/editor/EditorSession.hpp"
#include "yqlora/render/Camera3D.hpp"
#include "yqlora/scene/Scene.hpp"

#include <array>
#include <cmath>

namespace yqlora::editor {

namespace {

// Треугольник "верх камеры" в долях половины кадра: ширина, низ и вершина.
constexpr float kUpMarkerWidth = 0.5f;
constexpr float kUpMarkerBase = 1.1f;
constexpr float kUpMarkerTop = 1.6f;

void drawCamera(ImDrawList& list, const GizmoContext& context, const Camera3D& camera, ImU32 color)
{
    // Пирамида глубиной в полметра, пропорции экрана 16:9.
    constexpr float kDepth = 0.5f;
    constexpr float kAspect = 16.0f / 9.0f;
    const float halfHeight = kDepth * std::tan(glm::radians(camera.fovDegrees()) * 0.5f);
    const float halfWidth = halfHeight * kAspect;

    const Mat4& world = camera.worldMatrix();
    const auto toWorld = [&](float x, float y, float z) {
        return Vec3(world * Vec4(x, y, z, 1.0f));
    };
    const Vec3 apex = toWorld(0.0f, 0.0f, 0.0f);
    const std::array<Vec3, 4> corners = {
        toWorld(-halfWidth, -halfHeight, -kDepth),
        toWorld(halfWidth, -halfHeight, -kDepth),
        toWorld(halfWidth, halfHeight, -kDepth),
        toWorld(-halfWidth, halfHeight, -kDepth),
    };

    for (std::size_t i = 0; i < corners.size(); ++i) {
        drawWorldLine(list, context, apex, corners[i], color);
        drawWorldLine(list, context, corners[i], corners[(i + 1) % corners.size()], color);
    }
    // Треугольник над верхней гранью показывает, где у камеры верх.
    const Vec3 left = toWorld(-halfWidth * kUpMarkerWidth, halfHeight * kUpMarkerBase, -kDepth);
    const Vec3 right = toWorld(halfWidth * kUpMarkerWidth, halfHeight * kUpMarkerBase, -kDepth);
    const Vec3 top = toWorld(0.0f, halfHeight * kUpMarkerTop, -kDepth);
    drawWorldLine(list, context, left, right, color);
    drawWorldLine(list, context, right, top, color);
    drawWorldLine(list, context, top, left, color);
}

void drawCamerasRecursive(ImDrawList& list, const GizmoContext& context, const Node& node, NodeId selected)
{
    if (const auto* camera = dynamic_cast<const Camera3D*>(&node)) {
        const EditorColors& colors = context.theme.colors();
        const ImU32 color = toImU32(node.id() == selected ? colors.selectionOutline : colors.cameraGizmo);
        drawCamera(list, context, *camera, color);
    }
    for (const auto& child : node.children()) {
        drawCamerasRecursive(list, context, *child, selected);
    }
}

} // namespace

bool CameraGizmo::draw(GizmoContext& context)
{
    const Node* root = context.session.scene().root();
    if (!root) {
        return false;
    }
    const ImVec2& origin = context.origin;
    const ImVec2& size = context.size;
    ImDrawList& list = *ImGui::GetWindowDrawList();
    list.PushClipRect(origin, ImVec2(origin.x + size.x, origin.y + size.y), true);
    drawCamerasRecursive(list, context, *root, context.session.selection().selected());
    list.PopClipRect();

    return false;
}

} // namespace yqlora::editor
