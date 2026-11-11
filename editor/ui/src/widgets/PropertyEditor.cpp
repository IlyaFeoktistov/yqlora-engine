#include "yqlora/editor/ui/widgets/PropertyEditor.hpp"

#include "yqlora/editor/ui/style/Icons.hpp"
#include "yqlora/editor/ui/style/Theme.hpp"
#include "yqlora/editor/ui/widgets/Checkbox.hpp"
#include "yqlora/editor/ui/widgets/ChoiceFields.hpp"
#include "yqlora/editor/ui/widgets/DragDrop.hpp"
#include "yqlora/editor/ui/widgets/EventTargetsField.hpp"
#include "yqlora/editor/ui/widgets/InputControlField.hpp"
#include "yqlora/editor/ui/widgets/LayerFields.hpp"
#include "yqlora/editor/ui/widgets/NumberFields.hpp"
#include "yqlora/editor/ui/widgets/ReferenceField.hpp"
#include "yqlora/editor/ui/widgets/ResourceField.hpp"

#include "yqlora/editor/EventTargets.hpp"
#include "yqlora/editor/ReferenceFields.hpp"

#include "yqlora/editor/PropertyValues.hpp"
#include "yqlora/editor/ResourceLabel.hpp"

#include "yqlora/i18n/Translate.hpp"
#include "yqlora/project/SettingsSection.hpp"
#include "yqlora/reflection/Property.hpp"
#include "yqlora/reflection/TypeRegistry.hpp"
#include "yqlora/resource/ResourceDatabase.hpp"
#include "yqlora/scene/Scene.hpp"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <cmath>
#include <optional>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace yqlora::editor {

namespace {

constexpr const char* kRowMenu = "##propertyMenu";
// Число вектора, для которого открыто меню строки; -1 - всё значение.
// Хранится в памяти окна ImGui, как kClickedComponent.
constexpr const char* kMenuComponent = "##menuComponent";
constexpr int kWholeValue = -1;

// Значение элемента и кнопка его удаления.
constexpr int kListColumnCount = 2;
// Подпись поля записи и его значение.
constexpr int kRecordColumnCount = 2;
// Подпись элемента (слота) и его значение; подпись уже значения.
constexpr int kLabeledListColumnCount = 2;
constexpr float kLabeledListLabelWeight = 0.4f;

// Сколько меняется значение за точку движения мыши.
constexpr float kAngleDragSpeed = 0.5f;
constexpr float kFloatDragSpeed = 0.05f;
constexpr float kVectorDragSpeed = 0.02f;
// Одиночное число тянется не медленнее этой доли своей величины за точку:
// освещённость солнца (100 000 лк) меняется так же заметно, как множитель 0.5.
constexpr float kRelativeDragStep = 0.01f;

// value - текущее значение: шаг растёт вместе с ним.
float dragSpeed(const Property& property, float value)
{
    switch (property.hint()) {
    case PropertyHint::Angle: return kAngleDragSpeed;
    default:
        return property.type() == PropertyType::Float ? std::max(kFloatDragSpeed, std::abs(value) * kRelativeDragStep)
                                                      : kVectorDragSpeed;
    }
}

// Подпись строки. У одиночного числа её можно тянуть, у остальных - просто текст.
void rowLabel(const Property& property, PropertyValue& value, PropertyResult& result)
{
    const std::string label = propertyLabel(property);
    // В узком окне подпись обрезается: полный текст - подсказкой.
    const bool clipped = ImGui::CalcTextSize(label.c_str()).x + 2.0f * ImGui::GetStyle().FramePadding.x
                         > ImGui::GetContentRegionAvail().x;
    if (property.type() == PropertyType::Float) {
        dragLabel(
            "##label",
            label.c_str(),
            ImGui::GetStyleColorVec4(ImGuiCol_Text),
            std::get<float>(value),
            dragSpeed(property, std::get<float>(value)),
            result
        );
    } else {
        // С тем же отступом, что у подписей, которые можно тянуть.
        ImGui::AlignTextToFramePadding();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetStyle().FramePadding.x);
        ImGui::TextUnformatted(label.c_str());
    }
    // Подсказка поля - ключ перевода или сам текст: tr без перевода вернёт его же.
    const std::string& tooltip = property.attributes().tooltip;
    if (!tooltip.empty()) {
        ImGui::SetItemTooltip("%s%s%s", clipped ? label.c_str() : "", clipped ? "\n" : "", tr(tooltip).c_str());
    } else if (clipped) {
        ImGui::SetItemTooltip("%s", label.c_str());
    }
}

void valueField(
    const Property& property,
    PropertyType type,
    PropertyValue& value,
    const PropertyEditorContext& context,
    PropertyResult& result
);

// Поля записи таблицей, как свойства в инспекторе. Описание полей - у свойства
// (Property::fields): и у самой записи, и у списка записей.
void recordField(
    const Property& property,
    RecordValue& record,
    const PropertyEditorContext& context,
    PropertyResult& result
)
{
    const auto& fields = property.fields();
    if (fields.size() != record.fields.size() || !ImGui::BeginTable("fields", kRecordColumnCount)) {
        return;
    }
    ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);
    for (std::size_t i = 0; i < fields.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(propertyLabel(fields[i]).c_str());
        ImGui::TableNextColumn();
        valueField(fields[i], fields[i].type(), record.fields[i], context, result);
        ImGui::PopID();
    }
    ImGui::EndTable();
}

// Числа значения в пределах PropertyAttributes::min и max: и само число, и
// числа вектора, и элементы списка чисел.
void clampToRange(const Property& property, PropertyValue& value)
{
    const PropertyAttributes& attributes = property.attributes();
    if (!attributes.min && !attributes.max) {
        return;
    }
    const auto clamp = [&](float number) {
        if (attributes.min) {
            number = std::max(number, *attributes.min);
        }
        if (attributes.max) {
            number = std::min(number, *attributes.max);
        }

        return number;
    };
    if (float* number = std::get_if<float>(&value)) {
        *number = clamp(*number);
    } else if (int* integer = std::get_if<int>(&value)) {
        *integer = static_cast<int>(clamp(static_cast<float>(*integer)));
    } else {
        for (float& component : vectorComponents(value)) {
            component = clamp(component);
        }
    }
}

// Элементы списка друг под другом, у каждого кнопка удаления, внизу кнопка добавления.
// Подсказка свойства относится к элементам: список цветов показывается цветами.
void listField(const Property& property, ListValue& list, const PropertyEditorContext& context, PropertyResult& result)
{
    if (property.hint() == PropertyHint::LayerNames) {
        result.changed = layerNamesField(list) || result.changed;

        return;
    }
    if (property.hint() == PropertyHint::CollisionMatrix) {
        result.changed = collisionMatrixField(list, context) || result.changed;

        return;
    }

    std::optional<std::size_t> removed;
    if (!list.items.empty() && ImGui::BeginTable("items", kListColumnCount)) {
        ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("remove", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFrameHeight());
        for (std::size_t i = 0; i < list.items.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            valueField(property, property.elementType(), list.items[i], context, result);
            ImGui::TableNextColumn();
            if (ImGui::Button(icons::kDelete)) {
                removed = i;
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    if (removed) {
        list.items.erase(list.items.begin() + static_cast<std::ptrdiff_t>(*removed));
        result.changed = true;
    }
    const std::string add = std::string(icons::kAdd) + " " + tr("editor.inspector.addItem");
    if (ImGui::Button(add.c_str(), ImVec2(-FLT_MIN, 0.0f))) {
        list.items.push_back(property.defaultElement());
        result.changed = true;
    }
    // На кнопку добавления бросают сразу несколько файлов или узлов: каждый
    // подходящий становится элементом, всё одной правкой.
    std::vector<PropertyValue> dropped;
    if (property.elementType() == PropertyType::Resource) {
        const auto fits = [&](std::span<const ResourceId> ids) {
            return !resourceItems(property, ids, context.resources, context.cache).empty();
        };
        if (const auto ids = acceptResourcesDrop(fits)) {
            dropped = resourceItems(property, *ids, context.resources, context.cache);
        }
    } else if (property.elementType() == PropertyType::Node) {
        const auto nodesOf = [&](std::span<const NodeId> ids) {
            std::vector<const Node*> nodes;
            for (const NodeId id : ids) {
                nodes.push_back(context.scene.find(id));
            }

            return nodes;
        };
        const auto fits = [&](std::span<const NodeId> ids) {
            return !nodeItems(property, nodesOf(ids), context.registry).empty();
        };
        if (const auto ids = acceptNodesDrop(fits)) {
            dropped = nodeItems(property, nodesOf(*ids), context.registry);
        }
    }
    if (!dropped.empty()) {
        list.items.insert(list.items.end(), dropped.begin(), dropped.end());
        result.changed = true;
    }
}

// Список, чьи строки задают подписи объекта (слоты по подмешам): строка на
// подпись, даже если элементов меньше; добавлять и удалять нельзя, их
// число - дело объекта. Лишние элементы видны под своими номерами.
void labeledListField(
    const Property& property,
    ListValue& list,
    std::span<const std::string> labels,
    const PropertyEditorContext& context,
    PropertyResult& result
)
{
    const std::size_t rows = std::max(list.items.size(), labels.size());
    if (!ImGui::BeginTable("items", kLabeledListColumnCount)) {
        return;
    }
    ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthStretch, kLabeledListLabelWeight);
    ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch, 1.0f);
    for (std::size_t i = 0; i < rows; ++i) {
        ImGui::PushID(static_cast<int>(i));
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        const bool named = i < labels.size() && !labels[i].empty();
        const std::string label = named ? labels[i] : tr("editor.inspector.element") + " " + std::to_string(i);
        ImGui::TextUnformatted(label.c_str());
        ImGui::SetItemTooltip("%s", label.c_str());
        ImGui::TableNextColumn();
        PropertyValue item = i < list.items.size() ? list.items[i] : property.defaultElement();
        PropertyResult itemResult;
        valueField(property, property.elementType(), item, context, itemResult);
        result.dragging = result.dragging || itemResult.dragging;
        if (itemResult.changed) {
            // Элементы до этой строки появляются в списке пустыми.
            while (list.items.size() <= i) {
                list.items.push_back(property.defaultElement());
            }
            list.items[i] = std::move(item);
            result.changed = true;
        }
        ImGui::PopID();
    }
    ImGui::EndTable();
}

void valueField(
    const Property& property,
    PropertyType type,
    PropertyValue& value,
    const PropertyEditorContext& context,
    PropertyResult& result
)
{
    // Числа векторов - с постоянным шагом: у положения величина не мера шага.
    const float speed = dragSpeed(property, 0.0f);
    switch (type) {
    case PropertyType::Bool:
        result.changed = checkbox("##value", &std::get<bool>(value), context.metrics.checkboxPadding) || result.changed;
        break;
    case PropertyType::Int:
        if (property.hint() == PropertyHint::Layer) {
            result.changed = layerCombo(std::get<int>(value), context) || result.changed;
        } else if (property.hint() == PropertyHint::Layers) {
            result.changed = layerMaskCombo(std::get<int>(value), context) || result.changed;
        } else {
            ImGui::SetNextItemWidth(-FLT_MIN);
            result.changed = ImGui::DragInt("##value", &std::get<int>(value)) || result.changed;
        }
        break;
    case PropertyType::Float: floatField("##value", std::get<float>(value), -FLT_MIN, "%.3f", result); break;
    case PropertyType::String:
        if (property.hint() == PropertyHint::ComponentType) {
            result.changed = componentTypeCombo(std::get<std::string>(value), context.registry) || result.changed;
        } else if (property.hint() == PropertyHint::InputControlName) {
            result.changed = inputControlField(std::get<std::string>(value), context.input) || result.changed;
        } else {
            ImGui::SetNextItemWidth(-FLT_MIN);
            result.changed = ImGui::InputText("##value", &std::get<std::string>(value)) || result.changed;
        }
        break;
    case PropertyType::Vector2:
        vectorFields(&std::get<Vec2>(value).x, Vec2::length(), speed, context.colors, result);
        break;
    case PropertyType::Vector3:
        if (property.hint() == PropertyHint::Color) {
            ImGui::SetNextItemWidth(-FLT_MIN);
            result.changed = ImGui::ColorEdit3("##value", &std::get<Vec3>(value).x) || result.changed;
        } else {
            vectorFields(&std::get<Vec3>(value).x, Vec3::length(), speed, context.colors, result);
        }
        break;
    case PropertyType::Vector4:
        if (property.hint() == PropertyHint::Color) {
            ImGui::SetNextItemWidth(-FLT_MIN);
            result.changed = ImGui::ColorEdit4("##value", &std::get<Vec4>(value).x) || result.changed;
        } else {
            vectorFields(&std::get<Vec4>(value).x, Vec4::length(), speed, context.colors, result);
        }
        break;
    case PropertyType::Enum: result.changed = enumCombo(property, std::get<EnumValue>(value)) || result.changed; break;
    case PropertyType::Node:
        result.changed = nodeField(property, std::get<NodeRef>(value), context) || result.changed;
        break;
    case PropertyType::Component:
        result.changed = componentField(property, std::get<ComponentRef<>>(value), context) || result.changed;
        break;
    case PropertyType::Resource:
        if (const PropertyAttributes& attributes = property.attributes(); !attributes.resourceKind.empty()) {
            result.changed = resourceField(
                                 attributes.resourceKind,
                                 attributes.resourceUse,
                                 std::get<ResourceRef>(value),
                                 context
                             )
                             || result.changed;
        } else {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(resourceLabel(std::get<ResourceRef>(value), context.resources).c_str());
        }
        break;
    case PropertyType::List: listField(property, std::get<ListValue>(value), context, result); break;
    case PropertyType::Record: recordField(property, std::get<RecordValue>(value), context, result); break;
    }
    if (result.changed) {
        clampToRange(property, value);
    }
}

// Пункты для одного числа вектора: сбросить, скопировать, вставить только его.
void componentMenuItems(
    PropertyValue& value,
    int component,
    const std::optional<PropertyValue>& initial,
    PropertyResult& result
)
{
    float& number = vectorComponents(value)[static_cast<std::size_t>(component)];
    const std::string name = kComponentNames[component];
    std::optional<PropertyValue> initialValue = initial;
    const auto initialComponents = initialValue ? vectorComponents(*initialValue) : std::span<float>();
    const bool hasInitial = static_cast<std::size_t>(component) < initialComponents.size();
    const float initialNumber = hasInitial ? initialComponents[static_cast<std::size_t>(component)] : 0.0f;

    const std::string reset = tr("editor.inspector.resetPart") + " " + name;
    if (ImGui::MenuItem(reset.c_str(), nullptr, false, hasInitial && initialNumber != number)) {
        number = initialNumber;
        result.changed = true;
    }
    const std::string copy = tr("editor.inspector.copyPart") + " " + name;
    if (ImGui::MenuItem(copy.c_str())) {
        ImGui::SetClipboardText(numberToText(number).c_str());
    }
    const char* clipboard = ImGui::GetClipboardText();
    const auto pasted = clipboard ? numberFromText(clipboard) : std::nullopt;
    const std::string paste = tr("editor.inspector.pastePart") + " " + name;
    if (ImGui::MenuItem(paste.c_str(), nullptr, false, pasted.has_value())) {
        number = *pasted;
        result.changed = true;
    }
}

// Правый клик по строке свойства: сбросить к значению по умолчанию,
// скопировать, вставить - всё значение, а у вектора, если кликнули по числу,
// сначала то же для этого числа. Новое значение уходит в result, как от поля,
// поэтому правку можно отменить.
void rowMenu(
    const Property& property,
    PropertyValue& value,
    float rowTop,
    float rowBottom,
    PropertyResult& result,
    const std::function<std::optional<PropertyValue>()>& defaults,
    const PropertyOverrides* overrides
)
{
    ImGuiStorage& storage = *ImGui::GetStateStorage();
    const ImGuiID clickedKey = ImGui::GetID(kClickedComponent);
    const ImGuiID menuKey = ImGui::GetID(kMenuComponent);
    const ImVec2 rowMin(ImGui::GetWindowPos().x, rowTop);
    const ImVec2 rowMax(ImGui::GetWindowPos().x + ImGui::GetWindowWidth(), rowBottom);
    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && ImGui::IsMouseHoveringRect(rowMin, rowMax)
        && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
        storage.SetInt(menuKey, storage.GetInt(clickedKey, kWholeValue));
        ImGui::OpenPopup(kRowMenu);
    }
    storage.SetInt(clickedKey, kWholeValue);
    if (!ImGui::BeginPopup(kRowMenu)) {
        return;
    }
    const auto initial = defaults();
    const int component = storage.GetInt(menuKey, kWholeValue);
    if (component != kWholeValue && static_cast<std::size_t>(component) < vectorComponents(value).size()) {
        componentMenuItems(value, component, initial, result);
        ImGui::Separator();
    }
    if (ImGui::MenuItem(tr("editor.inspector.reset").c_str(), nullptr, false, initial && *initial != value)) {
        value = *initial;
        result.changed = true;
    }
    if (ImGui::MenuItem(tr("editor.inspector.copy").c_str())) {
        ImGui::SetClipboardText(valueToText(property, value).c_str());
    }
    const char* clipboard = ImGui::GetClipboardText();
    const auto pasted = clipboard ? valueFromText(property, clipboard) : std::nullopt;
    if (ImGui::MenuItem(tr("editor.inspector.paste").c_str(), nullptr, false, pasted.has_value())) {
        value = *pasted;
        result.changed = true;
    }
    if (overrides && overrides->overridden(property)) {
        ImGui::Separator();
        if (ImGui::MenuItem(tr("editor.prefab.revert").c_str())) {
            overrides->revert(property);
        }
        if (ImGui::MenuItem(tr("editor.prefab.apply").c_str(), nullptr, false, bool(overrides->apply))) {
            overrides->apply(property);
        }
    }
    ImGui::EndPopup();
}

// Подпись переопределённого свойства - цветом переопределения.
class OverrideMark {
public:
    OverrideMark(const Property& property, const PropertyOverrides* overrides, const PropertyEditorContext& context)
        : m_marked(overrides && overrides->overridden(property))
    {
        if (m_marked) {
            ImGui::PushStyleColor(ImGuiCol_Text, toImVec4(context.colors.prefabOverride));
        }
    }

    ~OverrideMark()
    {
        end();
    }

    OverrideMark(const OverrideMark&) = delete;
    OverrideMark& operator=(const OverrideMark&) = delete;

    void end()
    {
        if (std::exchange(m_marked, false)) {
            ImGui::PopStyleColor();
        }
    }

private:
    bool m_marked;
};

// Запись одним свойством (не элемент списка): раскрываемый узел в колонке
// подписей, а её поля - строками той же таблицы с отступом, на всю ширину
// значений, как обычные свойства.
void recordRows(
    const Property& property,
    RecordValue& record,
    const PropertyEditorContext& context,
    PropertyResult& result
)
{
    const auto& fields = property.fields();
    const bool
        open = ImGui::TreeNodeEx("##record", ImGuiTreeNodeFlags_SpanAllColumns, "%s", propertyLabel(property).c_str());
    if (!open || fields.size() != record.fields.size()) {
        if (open) {
            ImGui::TreePop();
        }

        return;
    }
    for (std::size_t i = 0; i < fields.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        rowLabel(fields[i], record.fields[i], result);
        ImGui::TableNextColumn();
        valueField(fields[i], fields[i].type(), record.fields[i], context, result);
        ImGui::PopID();
    }
    ImGui::TreePop();
}

template <typename Object>
PropertyResult drawPropertyRow(
    const Property& property,
    Object& object,
    const PropertyEditorContext& context,
    const PropertyOverrides* overrides
)
{
    PropertyResult result;
    PropertyValue value = property.get(object);

    ImGui::PushID(property.name().c_str());
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    const float rowTop = ImGui::GetCursorScreenPos().y;
    OverrideMark mark(property, overrides, context);
    if (property.type() == PropertyType::Record) {
        recordRows(property, std::get<RecordValue>(value), context, result);
        mark.end();
        rowMenu(
            property,
            value,
            rowTop,
            ImGui::GetItemRectMax().y,
            result,
            [&] {
                return defaultValue(context.registry, object, property);
            },
            overrides
        );
        ImGui::PopID();
        result.value = std::move(value);

        return result;
    }
    rowLabel(property, value, result);
    mark.end();
    ImGui::TableNextColumn();
    std::vector<std::string> labels;
    if constexpr (std::is_same_v<Object, Component>) {
        if (property.elementLabels()) {
            labels = property.elementLabels()(object);
        }
    }
    if (!labels.empty()) {
        labeledListField(property, std::get<ListValue>(value), labels, context, result);
    } else if (property.hint() == PropertyHint::EventTargets && std::holds_alternative<ListValue>(value)) {
        std::span<const EventParameter> parameters;
        if constexpr (std::is_same_v<Object, Component>) {
            parameters = eventParametersOf(object, property.name(), context.registry);
        }
        result.changed = eventTargetsField(property, std::get<ListValue>(value), parameters, context) || result.changed;
    } else {
        valueField(property, property.type(), value, context, result);
    }
    const float rowBottom = ImGui::GetItemRectMax().y;
    rowMenu(
        property,
        value,
        rowTop,
        rowBottom,
        result,
        [&] {
            return defaultValue(context.registry, object, property);
        },
        overrides
    );
    // Подпись числа тянут мимо поля значения: пределы и для неё.
    if (result.changed) {
        clampToRange(property, value);
    }
    ImGui::PopID();

    result.value = std::move(value);

    return result;
}

} // namespace

PropertyResult drawProperty(
    const Property& property,
    Node& node,
    const PropertyEditorContext& context,
    const PropertyOverrides* overrides
)
{
    return drawPropertyRow(property, node, context, overrides);
}

PropertyResult drawProperty(
    const Property& property,
    Component& component,
    const PropertyEditorContext& context,
    const PropertyOverrides* overrides
)
{
    return drawPropertyRow(property, component, context, overrides);
}

PropertyResult drawProperty(const Property& property, SettingsSection& section, const PropertyEditorContext& context)
{
    return drawPropertyRow(property, section, context, nullptr);
}

std::string propertyLabel(const Property& property)
{
    if (!property.attributes().displayName.empty()) {
        return property.attributes().displayName;
    }
    const std::string& name = property.name();
    const std::string key = property.labelKey();
    std::string label = tr(key);
    if (label == key) {
        label = name;
        if (!label.empty()) {
            label[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(label[0])));
        }
    }

    return label;
}

} // namespace yqlora::editor
