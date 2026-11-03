#include "yqlora/editor/ui/gizmos/LightGizmo.hpp"

#include "yqlora/editor/ui/gizmos/GizmoDrawing.hpp"
#include "yqlora/editor/ui/style/Icons.hpp"
#include "yqlora/editor/ui/style/NodeIcons.hpp"
#include "yqlora/editor/ui/style/Theme.hpp"

#include "yqlora/editor/EditorSession.hpp"
#include "yqlora/render/SpotLight3D.hpp"
#include "yqlora/scene/Scene.hpp"

#include <array>
#include <cmath>
#include <numbers>

namespace yqlora::editor {

namespace {

// Отрезков в окружности: глазу она уже гладкая.
constexpr int kCircleSegments = 48;
// Лучи направленного света: длина в метрах и радиус пучка вокруг оси.
constexpr float kRayLength = 1.5f;
constexpr float kRayBundleRadius = 0.2f;
// Линий от вершины конуса прожектора до его края.
constexpr int kConeEdges = 4;

// Окружность радиуса radius вокруг center в плоскости осей a и b.
void drawCircle(
    ImDrawList& list,
    const GizmoContext& context,
    const Vec3& center,
    const Vec3& a,
    const Vec3& b,
    float radius,
    ImU32 color
)
{
    constexpr float kFullTurn = 2.0f * std::numbers::pi_v<float>;
    Vec3 previous = center + a * radius;
    for (int i = 1; i <= kCircleSegments; ++i) {
        const float angle = kFullTurn * static_cast<float>(i) / kCircleSegments;
        const Vec3 next = center + (a * std::cos(angle) + b * std::sin(angle)) * radius;
        drawWorldLine(list, context, previous, next, color);
        previous = next;
    }
}

void drawDirectional(ImDrawList& list, const GizmoContext& context, const Light3D& light, ImU32 color)
{
    constexpr float kFullTurn = 2.0f * std::numbers::pi_v<float>;
    const Vec3 position = light.worldPosition();
    const Vec3 forward = light.worldForward();
    drawWorldLine(list, context, position, position + forward * kRayLength, color);
    for (int i = 0; i < kConeEdges; ++i) {
        const float angle = kFullTurn * static_cast<float>(i) / kConeEdges;
        const Vec3 start = position
                           + (light.worldRight() * std::cos(angle) + light.worldUp() * std::sin(angle))
                                 * kRayBundleRadius;
        drawWorldLine(list, context, start, start + forward * kRayLength, color);
    }
}

void drawPoint(ImDrawList& list, const GizmoContext& context, const PointLight3D& light, ImU32 color)
{
    // Три большие окружности сферы дальности, по осям мира.
    const Vec3 center = light.worldPosition();
    const Vec3 x(1.0f, 0.0f, 0.0f);
    const Vec3 y(0.0f, 1.0f, 0.0f);
    const Vec3 z(0.0f, 0.0f, 1.0f);
    drawCircle(list, context, center, x, y, light.range(), color);
    drawCircle(list, context, center, y, z, light.range(), color);
    drawCircle(list, context, center, z, x, light.range(), color);
}

void drawSpot(ImDrawList& list, const GizmoContext& context, const SpotLight3D& light, ImU32 color)
{
    constexpr float kFullTurn = 2.0f * std::numbers::pi_v<float>;
    const Vec3 apex = light.worldPosition();
    const Vec3 forward = light.worldForward();
    const Vec3 right = light.worldRight();
    const Vec3 up = light.worldUp();
    // Край конуса - там, где его достаёт дальность.
    const auto edge = [&](float angleDegrees) {
        const float angle = glm::radians(angleDegrees);

        return std::pair(apex + forward * (light.range() * std::cos(angle)), light.range() * std::sin(angle));
    };
    const auto [outerCenter, outerRadius] = edge(light.outerAngleDegrees());
    const auto [innerCenter, innerRadius] = edge(light.innerAngleDegrees());
    drawCircle(list, context, outerCenter, right, up, outerRadius, color);
    drawCircle(list, context, innerCenter, right, up, innerRadius, color);
    for (int i = 0; i < kConeEdges; ++i) {
        const float angle = kFullTurn * static_cast<float>(i) / kConeEdges;
        const Vec3 rim = outerCenter + (right * std::cos(angle) + up * std::sin(angle)) * outerRadius;
        drawWorldLine(list, context, apex, rim, color);
    }
}

void drawShape(ImDrawList& list, const GizmoContext& context, const Light3D& light, ImU32 color)
{
    switch (light.kind()) {
    case LightKind::Directional: drawDirectional(list, context, light, color); break;
    case LightKind::Point: drawPoint(list, context, static_cast<const PointLight3D&>(light), color); break;
    case LightKind::Spot: drawSpot(list, context, static_cast<const SpotLight3D&>(light), color); break;
    }
}

void drawLightsRecursive(ImDrawList& list, const GizmoContext& context, const Node& node, NodeId selected)
{
    if (const auto* light = dynamic_cast<const Light3D*>(&node)) {
        const EditorColors& colors = context.theme.colors();
        const bool isSelected = node.id() == selected;
        const ImU32 color = toImU32(isSelected ? colors.selectionOutline : colors.lightGizmo);
        if (isSelected) {
            drawShape(list, context, *light, color);
        }
        if (const auto screen = worldToScreen(context, light->worldPosition())) {
            const char* icon = nodeIcon(light->typeName());
            const ImVec2 size = ImGui::CalcTextSize(icon);
            list.AddText(ImVec2(screen->x - size.x * 0.5f, screen->y - size.y * 0.5f), color, icon);
        }
    }
    for (const auto& child : node.children()) {
        drawLightsRecursive(list, context, *child, selected);
    }
}

} // namespace

bool LightGizmo::draw(GizmoContext& context)
{
    const Node* root = context.session.scene().root();
    if (!root) {
        return false;
    }
    const ImVec2& origin = context.origin;
    const ImVec2& size = context.size;
    ImDrawList& list = *ImGui::GetWindowDrawList();
    list.PushClipRect(origin, ImVec2(origin.x + size.x, origin.y + size.y), true);
    drawLightsRecursive(list, context, *root, context.session.selection().selected());
    list.PopClipRect();

    return false;
}

} // namespace yqlora::editor
