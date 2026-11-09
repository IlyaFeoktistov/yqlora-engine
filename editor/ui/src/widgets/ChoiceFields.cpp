#include "yqlora/editor/ui/widgets/ChoiceFields.hpp"

#include "yqlora/i18n/Translate.hpp"
#include "yqlora/reflection/TypeRegistry.hpp"

#include <imgui.h>

#include <algorithm>
#include <string>
#include <vector>

namespace yqlora::editor {

namespace {

// Подпись значения перечисления на языке редактора; без перевода - его имя.
std::string enumValueLabel(const std::string& name)
{
    const std::string key = enumValueLabelKey(name);
    const std::string label = tr(key);

    return label == key ? name : label;
}

} // namespace

bool enumCombo(const Property& property, EnumValue& value)
{
    const auto& names = property.enumNames();
    const auto current = static_cast<std::size_t>(value.value);
    const std::string preview = value.value >= 0 && current < names.size() ? enumValueLabel(names[current])
                                                                           : std::to_string(value.value);

    bool changed = false;
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("##value", preview.c_str())) {
        for (std::size_t i = 0; i < names.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            const bool selected = ImGui::Selectable(enumValueLabel(names[i]).c_str(), i == current);
            ImGui::PopID();
            if (selected) {
                value.value = static_cast<int>(i);
                changed = true;
            }
        }
        ImGui::EndCombo();
    }

    return changed;
}

bool componentTypeCombo(std::string& value, const TypeRegistry& registry)
{
    bool changed = false;
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("##value", value.empty() ? tr("editor.inspector.none").c_str() : value.c_str())) {
        for (const auto& type : registry.types()) {
            if (type->kind() != TypeKind::Component || type->phases() == 0) {
                continue;
            }
            if (ImGui::Selectable(type->name().c_str(), type->name() == value)) {
                value = type->name();
                changed = true;
            }
            ImGui::SetItemTooltip("%s", tr(type->descriptionKey()).c_str());
        }
        ImGui::EndCombo();
    }

    return changed;
}

} // namespace yqlora::editor
