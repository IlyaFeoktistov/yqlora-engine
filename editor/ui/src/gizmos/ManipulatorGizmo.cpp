#include "yqlora/editor/ui/gizmos/ManipulatorGizmo.hpp"

#include "yqlora/editor/ui/gizmos/GizmoDrawing.hpp"
#include "yqlora/editor/ui/style/Icons.hpp"
#include "yqlora/editor/ui/style/Theme.hpp"

#include "yqlora/editor/EditorCamera.hpp"
#include "yqlora/editor/EditorSession.hpp"
#include "yqlora/editor/EditorSettings.hpp"
#include "yqlora/editor/Picking.hpp"
#include "yqlora/editor/SceneEditing.hpp"
#include "yqlora/editor/Snapping.hpp"
#include "yqlora/i18n/Translate.hpp"
#include "yqlora/scene/Node3D.hpp"
#include "yqlora/scene/NodeTree.hpp"
#include "yqlora/scene/Scene.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <imgui.h>
// ImGuizmo.h использует типы ImGui и должен идти после imgui.h.
#include <ImGuizmo.h>

#include <cfloat>
#include <format>
#include <string>

namespace yqlora::editor {

namespace {

// Панель инструментов в углу окна сцены.
constexpr float kToolbarMargin = 8.0f;
constexpr float kButtonSpacing = 2.0f;
// Между инструментами и переключателем осей.
constexpr float kGroupSpacing = 12.0f;

// Клавиши привязки: удерживаются, а не нажимаются, поэтому не действия меню.
constexpr ImGuiKey kVertexSnapKey = ImGuiKey_V;
// Вершина другого меша ловится, если она на экране ближе этого к курсору.
constexpr float kVertexSnapPixels = 30.0f;
constexpr float kVertexMarkerRadius = 5.0f;
constexpr float kStepFieldWidth = 80.0f;
constexpr float kMinStep = 0.001f;
constexpr float kStepDragSpeed = 0.01f;
constexpr float kAngleDragSpeed = 0.5f;
constexpr const char* kSnapPopup = "##snapSteps";

Vec2 mouseInView(const GizmoContext& context)
{
    const ImVec2 mouse = ImGui::GetMousePos();

    return {mouse.x - context.origin.x, mouse.y - context.origin.y};
}

void drawMarker(const GizmoContext& context, const Vec3& world, ImU32 color)
{
    if (const auto screen = worldToScreen(context, world)) {
        ImGui::GetWindowDrawList()->AddCircle(*screen, kVertexMarkerRadius, color, 0, 2.0f);
    }
}

// Мировая матрица узла с другим положением в мире.
Mat4 withWorldPosition(const Node3D& node, const Vec3& position)
{
    Mat4 world = node.worldMatrix();
    world[3] = Vec4(position, 1.0f);

    return world;
}

} // namespace

bool ManipulatorGizmo::draw(GizmoContext& context)
{
    const bool toolbarHovered = drawToolbar(context);
    if (context.mouseFree && !context.camera.flying()) {
        handleHotkeys();
    }

    return drawManipulator(context) || toolbarHovered;
}

bool ManipulatorGizmo::drawToolbar(const GizmoContext& context)
{
    struct Button {
        Tool tool;
        const char* icon;
        const char* labelKey;
        const char* hotkey;
    };
    static constexpr Button kButtons[] = {
        {Tool::Select, icons::kSelect, "editor.tool.select", "Q"},
        {Tool::Move, icons::kMove, "editor.tool.move", "W"},
        {Tool::Rotate, icons::kRotate, "editor.tool.rotate", "E"},
        {Tool::Scale, icons::kScale, "editor.tool.scale", "R"},
    };

    ImGui::SetCursorScreenPos(ImVec2(context.origin.x + kToolbarMargin, context.origin.y + kToolbarMargin));
    bool hovered = false;
    for (const Button& button : kButtons) {
        const bool active = m_tool == button.tool;
        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        }
        if (ImGui::Button(button.icon)) {
            m_tool = button.tool;
        }
        if (active) {
            ImGui::PopStyleColor();
        }
        hovered = hovered || ImGui::IsItemHovered();
        ImGui::SetItemTooltip("%s (%s)", tr(button.labelKey).c_str(), button.hotkey);
        ImGui::SameLine(0.0f, kButtonSpacing);
    }

    ImGui::SameLine(0.0f, kGroupSpacing);
    if (ImGui::Button(m_localSpace ? icons::kLocal : icons::kGlobal)) {
        m_localSpace = !m_localSpace;
    }
    hovered = hovered || ImGui::IsItemHovered();
    ImGui::SetItemTooltip(
        "%s: %s",
        tr(m_localSpace ? "editor.tool.local" : "editor.tool.global").c_str(),
        tr("editor.tool.spaceHint").c_str()
    );
    ImGui::SameLine(0.0f, kGroupSpacing);

    return drawSnapButtons(context) || hovered;
}

bool ManipulatorGizmo::drawSnapButtons(const GizmoContext& context)
{
    SnapSettings& snap = context.settings.snap;
    bool changed = false;
    const auto toggle = [&](const char* icon, bool& value) {
        // Цвет снимается по тому же состоянию, по которому положен: щелчок
        // меняет value между ними.
        const bool wasOn = value;
        if (wasOn) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        }
        if (ImGui::Button(icon)) {
            value = !value;
            changed = true;
        }
        if (wasOn) {
            ImGui::PopStyleColor();
        }
    };

    toggle(icons::kSnapGrid, snap.grid);
    bool hovered = ImGui::IsItemHovered();
    ImGui::SetItemTooltip("%s", tr("editor.tool.snapGrid").c_str());
    ImGui::SameLine(0.0f, 0.0f);
    if (ImGui::Button(icons::kDropDown)) {
        ImGui::OpenPopup(kSnapPopup);
    }
    hovered = hovered || ImGui::IsItemHovered();
    ImGui::SetItemTooltip("%s", tr("editor.tool.snapSteps").c_str());
    ImGui::SameLine(0.0f, kButtonSpacing);
    toggle(icons::kAlignToSurface, snap.alignToSurface);
    hovered = hovered || ImGui::IsItemHovered();
    ImGui::SetItemTooltip("%s", tr("editor.tool.alignToSurface").c_str());

    if (ImGui::BeginPopup(kSnapPopup)) {
        ImGui::SeparatorText(tr("editor.tool.moveStep").c_str());
        for (const float step : kMoveStepChoices) {
            const std::string label = std::format("{:g} {}", step, tr("editor.tool.meters"));
            if (ImGui::RadioButton(label.c_str(), snap.moveStep == step)) {
                snap.moveStep = step;
                changed = true;
            }
        }
        ImGui::SetNextItemWidth(kStepFieldWidth);
        changed = ImGui::DragFloat(
                      tr("editor.tool.customStep").c_str(),
                      &snap.moveStep,
                      kStepDragSpeed,
                      kMinStep,
                      FLT_MAX,
                      "%.3g",
                      ImGuiSliderFlags_AlwaysClamp
                  )
                  || changed;
        ImGui::SetNextItemWidth(kStepFieldWidth);
        changed = ImGui::DragFloat(
                      tr("editor.tool.rotateStep").c_str(),
                      &snap.rotateStepDegrees,
                      kAngleDragSpeed,
                      kMinStep,
                      FLT_MAX,
                      "%.3g",
                      ImGuiSliderFlags_AlwaysClamp
                  )
                  || changed;
        ImGui::SetNextItemWidth(kStepFieldWidth);
        changed = ImGui::DragFloat(
                      tr("editor.tool.scaleStep").c_str(),
                      &snap.scaleStep,
                      kStepDragSpeed,
                      kMinStep,
                      FLT_MAX,
                      "%.3g",
                      ImGuiSliderFlags_AlwaysClamp
                  )
                  || changed;
        ImGui::TextDisabled("%s", tr("editor.tool.snapKeys").c_str());
        ImGui::EndPopup();
    }
    if (changed) {
        context.settings.save();
    }

    return hovered;
}

void ManipulatorGizmo::handleHotkeys()
{
    // Пока печатают в поле ввода, буквы - это текст, а не инструменты.
    if (ImGui::GetIO().WantTextInput) {
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Q, false)) {
        m_tool = Tool::Select;
    } else if (ImGui::IsKeyPressed(ImGuiKey_W, false)) {
        m_tool = Tool::Move;
    } else if (ImGui::IsKeyPressed(ImGuiKey_E, false)) {
        m_tool = Tool::Rotate;
    } else if (ImGui::IsKeyPressed(ImGuiKey_R, false)) {
        m_tool = Tool::Scale;
    }
}

bool ManipulatorGizmo::drawManipulator(GizmoContext& context)
{
    auto* node = dynamic_cast<Node3D*>(context.session.selectedNode());
    if (m_vertexDrag && (!node || node->id() != m_vertexDrag->node)) {
        m_vertexDrag.reset();
    }
    if (!node || m_tool == Tool::Select || context.camera.flying()) {
        return false;
    }
    const bool vertexKey = !ImGui::GetIO().WantTextInput && ImGui::IsKeyDown(kVertexSnapKey);
    if (m_tool == Tool::Move && (m_vertexDrag || (vertexKey && context.mouseFree))) {
        return drawVertexSnap(context, *node);
    }

    ImGuizmo::OPERATION operation = ImGuizmo::TRANSLATE;
    switch (m_tool) {
    case Tool::Rotate: operation = ImGuizmo::ROTATE; break;
    case Tool::Scale: operation = ImGuizmo::SCALE; break;
    case Tool::Select:
    case Tool::Move: break;
    }
    // Масштаб в ImGuizmo бывает только по своим осям объекта.
    const ImGuizmo::MODE mode = m_localSpace || m_tool == Tool::Scale ? ImGuizmo::LOCAL : ImGuizmo::WORLD;

    // Доля высоты окна, которую занимает манипулятор. Стандартный (0.1) мелковат.
    constexpr float kManipulatorSize = 0.15f;
    ImGuizmo::SetGizmoSizeClipSpace(kManipulatorSize);
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(context.origin.x, context.origin.y, context.size.x, context.size.y);
    ImGuizmo::SetOrthographic(false);

    const SnapSettings& snap = context.settings.snap;
    const ImGuiIO& io = ImGui::GetIO();
    const bool surface = m_tool == Tool::Move && io.KeyCtrl && io.KeyShift;
    const bool grid = !surface && (snap.grid || io.KeyCtrl);
    // Поворот и масштаб идут шагами от начала перетаскивания; перемещение
    // привязывается ниже к самой сетке мира, а не к шагам от начала.
    float steps[3] = {0.0f, 0.0f, 0.0f};
    if (m_tool == Tool::Rotate) {
        steps[0] = snap.rotateStepDegrees;
    } else if (m_tool == Tool::Scale) {
        steps[0] = snap.scaleStep;
    }
    const bool stepped = grid && m_tool != Tool::Move;

    Mat4 world = node->worldMatrix();
    const bool moved = ImGuizmo::Manipulate(
        glm::value_ptr(context.view.view),
        glm::value_ptr(context.view.projection),
        operation,
        mode,
        glm::value_ptr(world),
        nullptr,
        stepped ? steps : nullptr
    );
    if (moved && m_tool == Tool::Move) {
        if (surface) {
            const Vec2 mouse = mouseInView(context);
            const Ray ray = context.camera.screenRay(mouse, {context.size.x, context.size.y});
            if (const auto hit = raycastSurfaces(context.session.scene(), ray, node)) {
                world = withWorldPosition(*node, hit->point);
                if (snap.alignToSurface) {
                    Vec3 scale;
                    Quat rotation;
                    Vec3 position;
                    Vec3 skew;
                    Vec4 perspective;
                    glm::decompose(world, scale, rotation, position, skew, perspective);
                    world = glm::translate(Mat4(1.0f), position) * glm::mat4_cast(alignUpTo(rotation, hit->normal))
                            * glm::scale(Mat4(1.0f), scale);
                }
            }
        } else if (grid) {
            world[3] = Vec4(snapToGrid(Vec3(world[3]), snap.moveStep), 1.0f);
        }
    }
    if (moved) {
        if (const auto values = node->localValuesForWorld(world)) {
            context.session.editing().setTransform(*node, *values);
        }
    }

    return ImGuizmo::IsOver() || ImGuizmo::IsUsing();
}

bool ManipulatorGizmo::drawVertexSnap(GizmoContext& context, Node3D& node)
{
    const Scene& scene = context.session.scene();
    const Mat4 viewProjection = context.view.projection * context.view.view;
    const Vec2 size(context.size.x, context.size.y);
    const Vec2 mouse = mouseInView(context);
    const auto inBranch = [&](const Node3D& other) {
        return isInBranch(node, other);
    };
    const ImU32 color = ImGui::GetColorU32(toImVec4(context.theme.colors().selectionOutline));

    if (!m_vertexDrag) {
        const auto source = nearestVertex(scene, viewProjection, size, mouse, inBranch);
        if (source) {
            drawMarker(context, source->world, color);
        }
        if (source && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            m_vertexDrag = VertexDrag{node.id(), node.transform().values(), source->world - node.worldPosition()};
        }

        return true;
    }

    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        finishVertexDrag(context, node);

        return true;
    }
    const auto target = nearestVertex(scene, viewProjection, size, mouse, [&](const Node3D& other) {
        return !inBranch(other);
    });
    if (target && target->screenDistance <= kVertexSnapPixels) {
        drawMarker(context, target->world, color);
        // Пока тянут, узел меняется напрямую; в историю - одна правка в конце.
        if (const auto values = node.localValuesForWorld(
                withWorldPosition(node, target->world - m_vertexDrag->offset)
            )) {
            node.transform().setValues(*values);
        }
    }

    return true;
}

void ManipulatorGizmo::finishVertexDrag(GizmoContext& context, Node3D& node)
{
    const TransformValues moved = node.transform().values();
    node.transform().setValues(m_vertexDrag->original);
    m_vertexDrag.reset();
    if (moved != node.transform().values()) {
        context.session.editing().setTransform(node, moved);
    }
}

} // namespace yqlora::editor
