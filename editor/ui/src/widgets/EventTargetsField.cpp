#include "yqlora/editor/ui/widgets/EventTargetsField.hpp"

#include "yqlora/editor/ui/style/Icons.hpp"
#include "yqlora/editor/ui/widgets/DragDrop.hpp"
#include "yqlora/editor/ui/widgets/ReferenceField.hpp"

#include "yqlora/editor/EventTargets.hpp"
#include "yqlora/editor/ReferenceFields.hpp"
#include "yqlora/i18n/Translate.hpp"
#include "yqlora/scene/Scene.hpp"

#include <imgui.h>

#include <optional>
#include <string>
#include <vector>

namespace yqlora::editor {

namespace {

// Поля записи EventTarget по порядку описания (YQLORA_RECORD).
constexpr std::size_t kTargetField = 0;
constexpr std::size_t kMethodField = 1;
constexpr std::size_t kFieldCount = 2;

std::string nodeLabel(const Node& node)
{
    return node.name() + " (" + std::string(node.typeName()) + ")";
}

// Сам узел или один из его компонентов; true, если выбрали другой.
bool objectCombo(const Node& node, ComponentRef<>& target)
{
    const Component* current = nullptr;
    for (const auto& component : node.components()) {
        if (component->id() == target.component) {
            current = component.get();
        }
    }
    const std::string preview = current ? componentLabel(*current) : nodeLabel(node);
    bool changed = false;
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("##object", preview.c_str())) {
        if (ImGui::Selectable(nodeLabel(node).c_str(), target.component == 0)) {
            changed = target.component != 0;
            target.component = 0;
        }
        for (const auto& component : node.components()) {
            ImGui::PushID(component.get());
            if (ImGui::Selectable(componentLabel(*component).c_str(), component->id() == target.component)) {
                changed = component->id() != target.component;
                target.component = component->id();
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }

    return changed;
}

bool methodCombo(
    std::string_view typeName,
    std::string& method,
    std::span<const EventParameter> parameters,
    const PropertyEditorContext& context
)
{
    const auto methods = eventMethods(typeName, parameters, context.registry);
    const std::string preview = method.empty() ? tr("editor.eventTargets.method") : method;
    bool changed = false;
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("##method", preview.c_str())) {
        if (methods.empty()) {
            ImGui::TextDisabled("%s", tr("editor.eventTargets.noMethods").c_str());
        }
        for (const std::string& name : methods) {
            if (ImGui::Selectable(name.c_str(), name == method)) {
                changed = name != method;
                method = name;
            }
        }
        ImGui::EndCombo();
    }

    return changed;
}

// Узлы, брошенные на "Добавить".
std::optional<std::vector<NodeId>> acceptDroppedNodes(const PropertyEditorContext& context)
{
    return acceptNodesDrop([&](std::span<const NodeId> ids) {
        return context.scene.find(ids.front()) != nullptr;
    });
}

} // namespace

bool eventTargetsField(
    const Property& property,
    ListValue& list,
    std::span<const EventParameter> parameters,
    const PropertyEditorContext& context
)
{
    bool changed = false;
    std::optional<std::size_t> removed;
    for (std::size_t i = 0; i < list.items.size(); ++i) {
        auto* record = std::get_if<RecordValue>(&list.items[i]);
        if (!record || record->fields.size() != kFieldCount) {
            continue;
        }
        auto& target = std::get<ComponentRef<>>(record->fields[kTargetField]);
        auto& method = std::get<std::string>(record->fields[kMethodField]);
        ImGui::PushID(static_cast<int>(i));
        ImGui::BeginGroup();
        NodeRef node{target.node};
        if (nodeField(property, node, context)) {
            // Другой узел - другие компоненты и методы: выбор начинается заново.
            target = {node.id, 0};
            method.clear();
            changed = true;
        }
        if (const Node* found = context.scene.find(node)) {
            if (objectCombo(*found, target)) {
                method.clear();
                changed = true;
            }
            const Component* component = target.component != 0 ? context.scene.find(target) : nullptr;
            const std::string_view typeName = component ? component->typeName() : found->typeName();
            changed = methodCombo(typeName, method, parameters, context) || changed;
        }
        if (ImGui::Button(icons::kDelete)) {
            removed = i;
        }
        ImGui::EndGroup();
        ImGui::Separator();
        ImGui::PopID();
    }
    if (removed) {
        list.items.erase(list.items.begin() + static_cast<std::ptrdiff_t>(*removed));
        changed = true;
    }
    const std::string add = std::string(icons::kAdd) + " " + tr("editor.inspector.addItem");
    if (ImGui::Button(add.c_str(), ImVec2(-FLT_MIN, 0.0f))) {
        list.items.push_back(property.defaultElement());
        changed = true;
    }
    // Узел, брошенный на "Добавить", сразу становится целью нового подписчика.
    if (const auto dropped = acceptDroppedNodes(context)) {
        for (const NodeId id : *dropped) {
            PropertyValue item = property.defaultElement();
            std::get<ComponentRef<>>(std::get<RecordValue>(item).fields[kTargetField]) = {id, 0};
            list.items.push_back(std::move(item));
        }
        changed = true;
    }

    return changed;
}

} // namespace yqlora::editor
