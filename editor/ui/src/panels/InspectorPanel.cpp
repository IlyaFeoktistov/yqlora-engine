#include "yqlora/editor/ui/panels/InspectorPanel.hpp"

#include "yqlora/editor/ui/widgets/DragDrop.hpp"
#include "yqlora/editor/ui/widgets/PropertyContext.hpp"
#include "yqlora/editor/ui/widgets/PropertyTable.hpp"

#include "yqlora/editor/EditorSession.hpp"
#include "yqlora/editor/Inspection.hpp"
#include "yqlora/editor/ReferenceFields.hpp"
#include "yqlora/editor/ResourceInspection.hpp"
#include "yqlora/i18n/Translate.hpp"
#include "yqlora/reflection/TypeRegistry.hpp"
#include "yqlora/render/Camera3D.hpp"

#include <imgui.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace yqlora::editor {

namespace {

// Свойства объекта сцены: правка идёт командой сцены, чтобы её можно было отменить.
template <typename Object>
bool drawObjectProperties(
    EditorSession& session,
    const PropertyEditorContext& widgets,
    const TypeInfo& type,
    Object& object
)
{
    return drawPropertyTable(
        session.registry(),
        widgets,
        type,
        object,
        [&session, &object](const Property& property, const PropertyValue& value) {
            session.editing().setProperty(object, property, value);
        }
    );
}

std::string joinTypeNames(const std::vector<const Component*>& components)
{
    std::string result;
    for (const Component* component : components) {
        result += result.empty() ? "" : ", ";
        result += component->typeName();
    }

    return result;
}

// Возвращает true, если какое-то значение тянут мышью. Компонент, который
// попросили снять, записывает в toRemove: снимать его посреди цикла по списку нельзя.
bool drawComponent(
    EditorSession& session,
    const PropertyEditorContext& widgets,
    const InspectedComponent& inspected,
    Component*& toRemove
)
{
    ImGui::PushID(&inspected.component);
    ImGui::Spacing();

    // Без крестика компонент, который снять нельзя.
    bool keep = true;
    const std::string header = inspected.typeName + "###component";
    const bool open = ImGui::CollapsingHeader(
        header.c_str(),
        inspected.removable ? &keep : nullptr,
        ImGuiTreeNodeFlags_DefaultOpen
    );
    // За заголовок компонент тянут в поле ссылки на компонент.
    componentDragSource(inspected.component, componentLabel(inspected.component));
    if (!inspected.requiredBy.empty()) {
        ImGui::SetItemTooltip(
            "%s %s",
            tr("editor.inspector.requiredBy").c_str(),
            joinTypeNames(inspected.requiredBy).c_str()
        );
    }

    bool dragging = false;
    if (open && inspected.type) {
        dragging = drawObjectProperties(session, widgets, *inspected.type, inspected.component);
    } else if (open) {
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled("%s", tr("editor.inspector.missingType").c_str());
        ImGui::PopTextWrapPos();
    }
    if (!keep) {
        toRemove = &inspected.component;
    }
    ImGui::PopID();

    return dragging;
}

void drawCameraRole(EditorSession& session, const Inspection& inspection)
{
    if (!inspection.camera) {
        return;
    }

    switch (*inspection.camera) {
    case CameraRole::Main: ImGui::TextDisabled("%s", tr("editor.inspector.camera.main").c_str()); break;
    case CameraRole::Secondary:
        if (ImGui::Button(tr("editor.inspector.camera.makeMain").c_str(), ImVec2(-FLT_MIN, 0.0f))) {
            session.editing().makeMainCamera(static_cast<Camera3D&>(inspection.node));
        }
        break;
    case CameraRole::Inactive: ImGui::TextDisabled("%s", tr("editor.inspector.camera.inactive").c_str()); break;
    }
    ImGui::SetItemTooltip("%s", tr("editor.inspector.camera.rule").c_str());
}

void drawAddableTypes(EditorSession& session, Node& node, const std::vector<const TypeInfo*>& types)
{
    for (const TypeInfo* type : types) {
        if (ImGui::Selectable(type->name().c_str())) {
            session.editing().addComponent(node, *type);
        }
        // У скриптов игры описания в переводах нет: ключ без перевода не показываем.
        const std::string key = type->descriptionKey();
        if (const std::string description = tr(key); description != key) {
            ImGui::SetItemTooltip("%s", description.c_str());
        }
    }
}

void drawAddComponent(EditorSession& session, const Inspection& inspection)
{
    const bool any = !inspection.addable.empty() || !inspection.addableScripts.empty();

    ImGui::Spacing();
    ImGui::BeginDisabled(!any);
    if (ImGui::Button(tr("editor.inspector.addComponent").c_str(), ImVec2(-FLT_MIN, 0.0f))) {
        ImGui::OpenPopup("AddComponent");
    }
    ImGui::EndDisabled();
    if (!any) {
        ImGui::SetItemTooltip("%s", tr("editor.inspector.noComponents").c_str());
    }

    if (ImGui::BeginPopup("AddComponent")) {
        drawAddableTypes(session, inspection.node, inspection.addable);
        if (!inspection.addableScripts.empty()) {
            ImGui::SeparatorText(tr("editor.inspector.scripts").c_str());
            drawAddableTypes(session, inspection.node, inspection.addableScripts);
        }
        ImGui::EndPopup();
    }
}

} // namespace

void InspectorPanel::draw(EditorContext& context)
{
    EditorSession& session = context.session;
    if (!beginWindow(context)) {
        ImGui::End();

        return;
    }

    if (const ResourceId resource = session.selection().resource()) {
        if (const auto inspected = inspectResource(session, resource)) {
            m_resource.draw(context, *inspected);
        }
        ImGui::End();

        return;
    }

    const std::optional<Inspection> inspection = inspect(session);
    if (!inspection) {
        ImGui::TextDisabled("%s", tr("editor.inspector.nothing").c_str());
        ImGui::End();

        return;
    }

    const PropertyEditorContext widgets = propertyEditorContext(context);
    ImGui::TextDisabled("%s", inspection->type.name().c_str());
    ImGui::SetItemTooltip("%s", tr(inspection->type.descriptionKey()).c_str());
    drawCameraRole(session, *inspection);
    bool dragging = drawObjectProperties(session, widgets, inspection->type, inspection->node);

    Component* toRemove = nullptr;
    for (const InspectedComponent& inspected : inspection->components) {
        dragging = drawComponent(session, widgets, inspected, toRemove) || dragging;
    }
    if (toRemove) {
        session.editing().removeComponent(inspection->node, *toRemove);
    }

    drawAddComponent(session, *inspection);
    ImGui::End();
    context.mouseWrapping = context.mouseWrapping || dragging;
}

} // namespace yqlora::editor
