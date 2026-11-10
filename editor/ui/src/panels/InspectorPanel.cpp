#include "yqlora/editor/ui/panels/InspectorPanel.hpp"

#include "yqlora/editor/ui/widgets/DragDrop.hpp"
#include "yqlora/editor/ui/widgets/PropertyContext.hpp"
#include "yqlora/editor/ui/widgets/PropertyTable.hpp"

#include "yqlora/editor/ui/style/Icons.hpp"
#include "yqlora/editor/ui/style/Theme.hpp"

#include "yqlora/editor/EditorSession.hpp"
#include "yqlora/editor/Inspection.hpp"
#include "yqlora/editor/PrefabEditing.hpp"
#include "yqlora/editor/ProjectFiles.hpp"
#include "yqlora/editor/ReferenceFields.hpp"
#include "yqlora/editor/ResourceInspection.hpp"
#include "yqlora/editor/ResourceLabel.hpp"
#include "yqlora/i18n/Translate.hpp"
#include "yqlora/reflection/TypeRegistry.hpp"
#include "yqlora/render/Camera3D.hpp"
#include "yqlora/scene/PrefabInstance.hpp"

#include <imgui.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace yqlora::editor {

namespace {

using PrefabAction = InspectorPanel::PrefabAction;

// Выбранный узел в экземпляре префаба: что в экземпляре переопределено и
// куда записать выбранное действие.
struct PrefabView {
    const InstanceOverrides& overrides;
    bool canApply = false;
    std::optional<PrefabAction>& action;
};

// Отметки и меню свойств узла (index - nullopt) или его компонента.
PropertyOverrides propertyOverrides(
    const PrefabView& view,
    const Node& node,
    const Component* component,
    std::optional<std::size_t> index
)
{
    const NodeId id = node.id();
    const auto request = [&view, id, index](PrefabAction::Kind kind) {
        return [&view, id, index, kind](const Property& property) {
            view.action = PrefabAction{kind, id, index, &property};
        };
    };
    PropertyOverrides result{
        [&view, &node, component](const Property& property) {
            return view.overrides.property(node, component, property.name());
        },
        request(PrefabAction::Kind::RevertProperty),
        {},
    };
    if (view.canApply) {
        result.apply = request(PrefabAction::Kind::ApplyProperty);
    }

    return result;
}

// Свойства объекта сцены: правка идёт командой сцены, чтобы её можно было отменить.
template <typename Object>
bool drawObjectProperties(
    EditorSession& session,
    const PropertyEditorContext& widgets,
    const TypeInfo& type,
    Object& object,
    const PropertyOverrides* overrides = nullptr
)
{
    return drawPropertyTable(
        session.registry(),
        widgets,
        type,
        object,
        [&session, &object](const Property& property, const PropertyValue& value) {
            session.editing().setProperty(object, property, value);
        },
        overrides
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
// Заголовок компонента экземпляра: цветом, если компонент добавлен или
// переопределён, с меню "Вернуть" и "Применить к префабу".
void componentPrefabMarks(const PrefabView& prefab, const Node& node, std::size_t index, bool added)
{
    ImGui::SetItemTooltip("%s", tr(added ? "editor.prefab.addedComponent" : "editor.prefab.overridden").c_str());
    if (!ImGui::BeginPopupContextItem("componentPrefab")) {
        return;
    }
    if (ImGui::MenuItem(tr("editor.prefab.revert").c_str())) {
        prefab.action = PrefabAction{PrefabAction::Kind::RevertComponent, node.id(), index, nullptr};
    }
    if (ImGui::MenuItem(tr("editor.prefab.apply").c_str(), nullptr, false, prefab.canApply)) {
        prefab.action = PrefabAction{PrefabAction::Kind::ApplyComponent, node.id(), index, nullptr};
    }
    ImGui::EndPopup();
}

bool drawComponent(
    EditorSession& session,
    const PropertyEditorContext& widgets,
    Node& node,
    const InspectedComponent& inspected,
    std::size_t index,
    const PrefabView* prefab,
    Component*& toRemove
)
{
    ImGui::PushID(&inspected.component);
    ImGui::Spacing();

    const bool added = prefab && prefab->overrides.addedComponent(node, inspected.component);
    const bool marked = added || (prefab && prefab->overrides.componentOverridden(node, inspected.component));
    // Без крестика компонент, который снять нельзя.
    bool keep = true;
    const std::string header = inspected.typeName + "###component";
    if (marked) {
        ImGui::PushStyleColor(ImGuiCol_Text, toImVec4(widgets.colors.prefabOverride));
    }
    const bool open = ImGui::CollapsingHeader(
        header.c_str(),
        inspected.removable ? &keep : nullptr,
        ImGuiTreeNodeFlags_DefaultOpen
    );
    if (marked) {
        ImGui::PopStyleColor();
        componentPrefabMarks(*prefab, node, index, added);
    }
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
        const auto overrides = prefab ? std::optional(propertyOverrides(*prefab, node, &inspected.component, index))
                                      : std::nullopt;
        dragging = drawObjectProperties(
            session,
            widgets,
            *inspected.type,
            inspected.component,
            overrides ? &*overrides : nullptr
        );
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

// Строка над свойствами корня экземпляра: его префаб и действия со всем
// экземпляром.
void drawInstanceHeader(EditorSession& session, const Node& root, const PrefabView& view, const EditorColors& colors)
{
    const PrefabInstance& instance = *root.prefabInstance();
    const auto request = [&](PrefabAction::Kind kind) {
        view.action = PrefabAction{kind, root.id(), std::nullopt, nullptr};
    };
    const ResourceDatabase* resources = session.project() ? &session.project()->resources() : nullptr;
    ImGui::TextColored(
        toImVec4(colors.prefabInstance),
        "%s  %s",
        icons::kPrefab,
        resourceLabel(ResourceRef{instance.prefab}, resources).c_str()
    );
    if (ImGui::SmallButton(tr("editor.prefab.open").c_str())) {
        request(PrefabAction::Kind::OpenPrefab);
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!view.overrides.any());
    if (ImGui::SmallButton(tr("editor.prefab.revertAll").c_str())) {
        request(PrefabAction::Kind::RevertInstance);
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!view.canApply);
    if (ImGui::SmallButton(tr("editor.prefab.applyAll").c_str())) {
        request(PrefabAction::Kind::ApplyInstance);
    }
    ImGui::EndDisabled();
    if (!view.canApply) {
        ImGui::SetItemTooltip("%s", tr("editor.prefab.readOnly").c_str());
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::SmallButton(tr("editor.prefab.unpack").c_str())) {
        request(PrefabAction::Kind::Unpack);
    }
    if (const std::size_t unused = view.overrides.unused(); unused > 0) {
        ImGui::TextColored(toImVec4(colors.warningText), "%s: %zu", tr("editor.prefab.unused").c_str(), unused);
        ImGui::SameLine();
        if (ImGui::SmallButton(tr("editor.prefab.removeUnused").c_str())) {
            request(PrefabAction::Kind::RemoveUnused);
        }
    }
    ImGui::Separator();
}

} // namespace

void InspectorPanel::runPrefabAction(EditorContext& context)
{
    if (!m_prefabAction) {
        return;
    }
    const PrefabAction action = *std::exchange(m_prefabAction, std::nullopt);
    EditorSession& session = context.session;
    PrefabEditing& prefabs = session.prefabEditing();
    Node* node = session.findNode(action.node);
    if (!node) {
        return;
    }
    Component* component = action.component && *action.component < node->components().size()
                               ? node->components()[*action.component].get()
                               : nullptr;
    switch (action.kind) {
    case PrefabAction::Kind::RevertProperty: prefabs.revertProperty(*node, component, *action.property); break;
    case PrefabAction::Kind::ApplyProperty: prefabs.applyProperty(*node, component, *action.property); break;
    case PrefabAction::Kind::RevertComponent:
        if (component) {
            prefabs.revertComponent(*node, *component);
        }
        break;
    case PrefabAction::Kind::ApplyComponent:
        if (component) {
            prefabs.applyComponent(*node, *component);
        }
        break;
    case PrefabAction::Kind::RevertInstance: prefabs.revertInstance(*node); break;
    case PrefabAction::Kind::ApplyInstance: prefabs.applyInstance(*node); break;
    case PrefabAction::Kind::RemoveUnused: prefabs.removeUnused(*node); break;
    case PrefabAction::Kind::Unpack: prefabs.unpack(*node); break;
    case PrefabAction::Kind::OpenPrefab: {
        const PrefabInstance* instance = node->prefabInstance();
        const auto path = instance && session.project() ? session.project()->resources().path(instance->prefab)
                                                        : std::nullopt;
        if (path && ProjectFiles::opensAsScene(*path)) {
            context.openScene(*path);
        }
        break;
    }
    }
}

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
    const EditorColors& colors = context.theme.colors();
    Node& node = inspection->node;
    // Узел экземпляра префаба: отметки переопределений и действия с ними.
    // Во время игры это копия Play, правки к префабу не относятся.
    const Node* root = session.playing() ? nullptr : instanceRootOf(node);
    std::optional<InstanceOverrides> overrides;
    std::optional<PrefabView> prefab;
    if (root && root->prefabInstance()->base) {
        overrides.emplace(*root, session.registry());
        prefab.emplace(PrefabView{*overrides, session.prefabEditing().canApply(*root), m_prefabAction});
    }
    if (prefab && root == &node) {
        drawInstanceHeader(session, *root, *prefab, colors);
    } else if (root == &node && root) {
        ImGui::TextColored(toImVec4(colors.warningText), "%s", tr("editor.prefab.notExpanded").c_str());
    } else if (overrides && overrides->addedNode(node)) {
        ImGui::TextColored(
            toImVec4(colors.prefabOverride),
            "%s  %s",
            icons::kAdd,
            tr("editor.prefab.addedNode").c_str()
        );
    }
    ImGui::TextDisabled("%s", inspection->type.name().c_str());
    ImGui::SetItemTooltip("%s", tr(inspection->type.descriptionKey()).c_str());
    drawCameraRole(session, *inspection);
    const auto nodeOverrides = prefab ? std::optional(propertyOverrides(*prefab, node, nullptr, std::nullopt))
                                      : std::nullopt;
    bool dragging = drawObjectProperties(
        session,
        widgets,
        inspection->type,
        node,
        nodeOverrides ? &*nodeOverrides : nullptr
    );

    Component* toRemove = nullptr;
    const PrefabView* view = prefab ? &*prefab : nullptr;
    for (std::size_t i = 0; i < inspection->components.size(); ++i) {
        dragging = drawComponent(session, widgets, node, inspection->components[i], i, view, toRemove) || dragging;
    }
    if (toRemove) {
        session.editing().removeComponent(node, *toRemove);
    }

    drawAddComponent(session, *inspection);
    ImGui::End();
    context.mouseWrapping = context.mouseWrapping || dragging;
    runPrefabAction(context);
}

} // namespace yqlora::editor
