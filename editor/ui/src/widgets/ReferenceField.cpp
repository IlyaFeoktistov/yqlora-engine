#include "yqlora/editor/ui/widgets/ReferenceField.hpp"

#include "yqlora/editor/ui/style/NodeIcons.hpp"
#include "yqlora/editor/ui/widgets/DragDrop.hpp"
#include "yqlora/editor/ui/widgets/ReferenceButton.hpp"

#include "yqlora/editor/Pings.hpp"
#include "yqlora/editor/ReferenceFields.hpp"
#include "yqlora/i18n/Translate.hpp"
#include "yqlora/scene/Scene.hpp"

#include <imgui.h>

#include <string>
#include <vector>

namespace yqlora::editor {

namespace {

constexpr const char* kNodePicker = "##nodePicker";
constexpr const char* kComponentPicker = "##componentPicker";
constexpr const char* kComponentQuestion = "##whichComponent";
// Окно выбора: ширина в строках поля.
constexpr float kPickerWidthLines = 14.0f;

// Путь узла от корня без самого корня: "Body/Arm/Hand"; у корня - его имя.
std::string nodePath(const Node& node)
{
    std::string path = node.name();
    for (const Node* parent = node.parent(); parent && parent->parent(); parent = parent->parent()) {
        path = parent->name() + "/" + path;
    }

    return path;
}

std::string emptyLabel(const std::string& typeName)
{
    return tr("editor.inspector.none") + " (" + typeName + ")";
}

void openPicker(const char* popup)
{
    ImGui::OpenPopup(popup);
}

} // namespace

bool nodeField(const Property& property, NodeRef& value, const PropertyEditorContext& context)
{
    static std::string search;
    const std::string& nodeType = property.attributes().nodeType;
    const std::string typeName = nodeType.empty() ? std::string(Node::staticTypeName()) : nodeType;
    const Node* target = context.scene.find(value);
    const std::string name = target ? nodePath(*target) : emptyLabel(typeName);
    const char* icon = nodeIcon(target ? target->typeName() : std::string_view(typeName));
    bool changed = false;

    const ReferenceClick click = referenceButton(std::string(icon) + "  " + name, name);
    if (click == ReferenceClick::Field && target && context.pings) {
        context.pings->node(target->id());
    } else if (click != ReferenceClick::None) {
        openPicker(kNodePicker);
    }
    const auto fits = [&](std::span<const NodeId> ids) {
        const Node* node = context.scene.find(ids.front());

        return node && nodeFits(property, *node, context.registry);
    };
    if (const auto dropped = acceptNodesDrop(fits)) {
        value.id = dropped->front();
        changed = true;
    }

    ImGui::SetNextWindowSize(ImVec2(ImGui::GetFrameHeight() * kPickerWidthLines, 0.0f));
    if (ImGui::BeginPopup(kNodePicker)) {
        ImGui::TextDisabled("%s: %s", tr("editor.resourceField.choose").c_str(), typeName.c_str());
        const auto choices = nodeChoices(property, context.scene, context.registry);
        std::vector<std::string> labels;
        int current = value.id == 0 ? ChoiceList::kNone : ChoiceList::kNothing;
        for (const Node* node : choices) {
            if (node->id() == value.id) {
                current = static_cast<int>(labels.size());
            }
            labels.push_back(std::string(nodeIcon(node->typeName())) + "  " + nodePath(*node));
        }
        const int chosen = drawChoiceList(labels, current, search);
        if (chosen == ChoiceList::kNone || chosen >= 0) {
            value.id = chosen >= 0 ? choices[static_cast<std::size_t>(chosen)]->id() : 0;
            changed = true;
        }
        ImGui::EndPopup();
    }

    return changed;
}

bool componentField(const Property& property, ComponentRef<>& value, const PropertyEditorContext& context)
{
    static std::string search;
    // Узел с несколькими подходящими компонентами: из каких выбирать.
    static std::vector<ComponentRef<>> asked;
    const std::string& componentType = property.attributes().componentType;
    const std::string typeName = componentType.empty() ? std::string(Component::staticTypeName()) : componentType;
    const Component* target = context.scene.find(value);
    const std::string name = target ? componentLabel(*target) : emptyLabel(typeName);
    bool changed = false;

    const ReferenceClick click = referenceButton(name, name);
    if (click == ReferenceClick::Field && target && target->owner() && context.pings) {
        context.pings->node(target->owner()->id());
    } else if (click != ReferenceClick::None) {
        openPicker(kComponentPicker);
    }
    const auto nodeFitsField = [&](std::span<const NodeId> ids) {
        const Node* node = context.scene.find(ids.front());

        return node && !fittingComponents(property, *node, context.registry).empty();
    };
    if (const auto dropped = acceptNodesDrop(nodeFitsField)) {
        const Node* node = context.scene.find(dropped->front());
        const auto fitting = fittingComponents(property, *node, context.registry);
        if (fitting.size() == 1) {
            value = {node->id(), fitting.front()->id()};
            changed = true;
        } else {
            asked.clear();
            for (const Component* component : fitting) {
                asked.push_back({node->id(), component->id()});
            }
            ImGui::OpenPopup(kComponentQuestion);
        }
    }
    const auto componentFitsField = [&](const DraggedComponent& dragged) {
        const Component* component = context.scene.find(ComponentRef<>{dragged.node, dragged.component});

        return component && componentFits(property, *component, context.registry);
    };
    if (const auto dropped = acceptComponentDrop(componentFitsField)) {
        value = {dropped->node, dropped->component};
        changed = true;
    }

    if (ImGui::BeginPopup(kComponentQuestion)) {
        ImGui::TextDisabled("%s", tr("editor.referenceField.whichComponent").c_str());
        for (const ComponentRef<>& candidate : asked) {
            const Component* component = context.scene.find(candidate);
            if (component && ImGui::Selectable(componentLabel(*component).c_str())) {
                value = candidate;
                changed = true;
            }
        }
        ImGui::EndPopup();
    }

    ImGui::SetNextWindowSize(ImVec2(ImGui::GetFrameHeight() * kPickerWidthLines, 0.0f));
    if (ImGui::BeginPopup(kComponentPicker)) {
        ImGui::TextDisabled("%s: %s", tr("editor.resourceField.choose").c_str(), typeName.c_str());
        const auto choices = componentChoices(property, context.scene, context.registry);
        std::vector<std::string> labels;
        int current = value.component == 0 ? ChoiceList::kNone : ChoiceList::kNothing;
        for (const Component* component : choices) {
            if (component->id() == value.component && component->owner()->id() == value.node) {
                current = static_cast<int>(labels.size());
            }
            labels.push_back(componentLabel(*component));
        }
        const int chosen = drawChoiceList(labels, current, search);
        if (chosen == ChoiceList::kNone) {
            value = {};
            changed = true;
        } else if (chosen >= 0) {
            const Component& component = *choices[static_cast<std::size_t>(chosen)];
            value = {component.owner()->id(), component.id()};
            changed = true;
        }
        ImGui::EndPopup();
    }

    return changed;
}

} // namespace yqlora::editor
