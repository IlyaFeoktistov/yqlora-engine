#include "yqlora/editor/ui/widgets/PropertyTable.hpp"

#include "yqlora/i18n/Translate.hpp"
#include "yqlora/reflection/TypeRegistry.hpp"

#include <imgui.h>

#include <algorithm>
#include <string>
#include <type_traits>
#include <vector>

namespace yqlora::editor {

namespace {

// Подпись и значение.
constexpr int kColumnCount = 2;
constexpr float kMinLabelWidth = 70.0f;
// Подписи не отнимают у значений больше этой доли ширины.
constexpr float kMaxLabelShare = 0.5f;

// По самой длинной подписи таблицы, с отступами по краям, как у подписей,
// которые можно тянуть; поля записи - со своим отступом под её узлом.
float labelColumnWidth(const std::vector<const Property*>& properties)
{
    float widest = 0.0f;
    for (const Property* property : properties) {
        widest = std::max(widest, ImGui::CalcTextSize(propertyLabel(*property).c_str()).x);
        if (property->type() == PropertyType::Record) {
            for (const Property& field : property->fields()) {
                const float indented = ImGui::CalcTextSize(propertyLabel(field).c_str()).x
                                       + ImGui::GetStyle().IndentSpacing;
                widest = std::max(widest, indented);
            }
        }
    }
    const float padded = widest + 2.0f * ImGui::GetStyle().FramePadding.x;
    const float limit = std::max(kMinLabelWidth, ImGui::GetContentRegionAvail().x * kMaxLabelShare);

    return std::clamp(padded, kMinLabelWidth, limit);
}

template <typename Object>
bool drawTable(
    const TypeRegistry& registry,
    const PropertyEditorContext& widgets,
    const TypeInfo& type,
    Object& object,
    const PropertyChanged& changed,
    const PropertyOverrides* overrides
)
{
    std::vector<const Property*> properties = registry.properties(type);
    std::erase_if(properties, [](const Property* property) {
        return !property->attributes().shown;
    });
    if (properties.empty() || !ImGui::BeginTable("properties", kColumnCount)) {
        return false;
    }
    ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, labelColumnWidth(properties));
    ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);

    bool dragging = false;
    const std::string* group = nullptr;
    for (const Property* property : properties) {
        // Новая группа - строка с её названием над её свойствами; свойство без
        // группы после группы - под чертой, чтобы не читалось как её часть.
        const std::string& propertyGroup = property->attributes().group;
        if (!propertyGroup.empty() && (!group || *group != propertyGroup)) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextDisabled("%s", tr(propertyGroup).c_str());
            ImGui::TableNextColumn();
            ImGui::Separator();
        } else if (propertyGroup.empty() && group && !group->empty()) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Separator();
            ImGui::TableNextColumn();
            ImGui::Separator();
        }
        group = &propertyGroup;
        PropertyResult result;
        if constexpr (std::is_same_v<Object, SettingsSection>) {
            result = drawProperty(*property, object, widgets);
        } else {
            result = drawProperty(*property, object, widgets, overrides);
        }
        if (result.changed) {
            changed(*property, result.value);
        }
        dragging = dragging || result.dragging;
    }
    ImGui::EndTable();

    return dragging;
}

} // namespace

bool drawPropertyTable(
    const TypeRegistry& registry,
    const PropertyEditorContext& widgets,
    const TypeInfo& type,
    Node& node,
    const PropertyChanged& changed,
    const PropertyOverrides* overrides
)
{
    return drawTable(registry, widgets, type, node, changed, overrides);
}

bool drawPropertyTable(
    const TypeRegistry& registry,
    const PropertyEditorContext& widgets,
    const TypeInfo& type,
    Component& component,
    const PropertyChanged& changed,
    const PropertyOverrides* overrides
)
{
    return drawTable(registry, widgets, type, component, changed, overrides);
}

bool drawPropertyTable(
    const TypeRegistry& registry,
    const PropertyEditorContext& widgets,
    const TypeInfo& type,
    SettingsSection& section,
    const PropertyChanged& changed
)
{
    return drawTable(registry, widgets, type, section, changed, nullptr);
}

} // namespace yqlora::editor
