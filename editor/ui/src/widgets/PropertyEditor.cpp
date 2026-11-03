#include "yqlora/editor/ui/widgets/PropertyEditor.hpp"

#include "yqlora/editor/ui/style/Icons.hpp"
#include "yqlora/editor/ui/style/Theme.hpp"
#include "yqlora/editor/ui/widgets/Checkbox.hpp"
#include "yqlora/editor/ui/widgets/ResourceField.hpp"

#include "yqlora/editor/PropertyValues.hpp"
#include "yqlora/editor/ResourceLabel.hpp"

#include "yqlora/i18n/Translate.hpp"
#include "yqlora/input/Input.hpp"
#include "yqlora/input/InputControl.hpp"
#include "yqlora/project/SettingsSection.hpp"
#include "yqlora/reflection/Property.hpp"
#include "yqlora/reflection/TypeRegistry.hpp"
#include "yqlora/resource/ResourceDatabase.hpp"
#include "yqlora/scene/Layers.hpp"
#include "yqlora/scene/Scene.hpp"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <bit>
#include <cctype>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace yqlora::editor {

namespace {

constexpr float kFastFactor = 10.0f; // с Shift
constexpr float kSlowFactor = 0.1f;  // с Ctrl

constexpr const char* kRowMenu = "##propertyMenu";
constexpr const char* kWaitingForControl = "##waitingForControl";
// Число вектора, по которому кликнули правой кнопкой в этом кадре, и то, для
// которого открыто меню; -1 - всё значение. Хранятся в памяти окна ImGui.
constexpr const char* kClickedComponent = "##clickedComponent";
constexpr const char* kMenuComponent = "##menuComponent";
constexpr int kWholeValue = -1;
constexpr const char* kComponentNames[] = {"X", "Y", "Z", "W"};

// Значение элемента и кнопка его удаления.
constexpr int kListColumnCount = 2;
// Подпись поля записи и его значение.
constexpr int kRecordColumnCount = 2;

// Сколько меняется значение за точку движения мыши.
constexpr float kAngleDragSpeed = 0.5f;
constexpr float kFloatDragSpeed = 0.05f;
constexpr float kVectorDragSpeed = 0.02f;

float dragSpeed(const Property& property)
{
    switch (property.hint()) {
    case PropertyHint::Angle: return kAngleDragSpeed;
    default: return property.type() == PropertyType::Float ? kFloatDragSpeed : kVectorDragSpeed;
    }
}

// Подпись, которую можно тянуть мышью, как в Unity. Меняет value и
// отмечает в result, что значение тянут.
void dragLabel(const char* id, const char* text, const ImVec4& color, float& value, float speed, PropertyResult& result)
{
    const ImVec2 textSize = ImGui::CalcTextSize(text);
    const ImVec2 padding = ImGui::GetStyle().FramePadding;
    const ImVec2 size(textSize.x + padding.x * 2.0f, ImGui::GetFrameHeight());
    const ImVec2 position = ImGui::GetCursorScreenPos();

    ImGui::InvisibleButton(id, size);
    const bool active = ImGui::IsItemActive();
    if (active || ImGui::IsItemHovered()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    }
    const ImVec2 textPosition(position.x + padding.x, position.y + padding.y);
    ImGui::GetWindowDrawList()->AddText(textPosition, ImGui::GetColorU32(color), text);

    if (!active) {
        return;
    }
    result.dragging = true;
    const ImGuiIO& io = ImGui::GetIO();
    const float factor = io.KeyShift ? kFastFactor : io.KeyCtrl ? kSlowFactor : 1.0f;
    const float delta = io.MouseDelta.x * speed * factor;
    if (delta != 0.0f) {
        value += delta;
        result.changed = true;
    }
}

// Поле числа без подписи заданной ширины. format влияет только на показ:
// введённое значение сохраняется с полной точностью.
void floatField(const char* id, float& value, float width, const char* format, PropertyResult& result)
{
    ImGui::SetNextItemWidth(width);
    if (ImGui::InputFloat(id, &value, 0.0f, 0.0f, format)) {
        result.changed = true;
    }
}

// Компоненты вектора в одну строку: у каждой своя цветная подпись.
void vectorFields(float* values, int count, float speed, const EditorColors& colors, PropertyResult& result)
{
    const ImVec4 axisColors[] =
        {toImVec4(colors.axisX), toImVec4(colors.axisY), toImVec4(colors.axisZ), toImVec4(colors.axisW)};

    const ImGuiStyle& style = ImGui::GetStyle();
    const float labelWidth = ImGui::CalcTextSize("X").x + style.FramePadding.x * 2.0f;
    const float total = ImGui::GetContentRegionAvail().x;
    const float spacing = style.ItemInnerSpacing.x;
    const float fieldWidth = (total - static_cast<float>(count) * labelWidth - static_cast<float>(count - 1) * spacing)
                             / static_cast<float>(count);

    // Правый клик по числу: меню строки предложит пункты и для него одного.
    const ImGuiID clickedKey = ImGui::GetID(kClickedComponent);
    const bool rightReleased = ImGui::IsMouseReleased(ImGuiMouseButton_Right);
    for (int i = 0; i < count; ++i) {
        ImGui::PushID(i);
        if (i > 0) {
            ImGui::SameLine(0.0f, spacing);
        }
        dragLabel("##drag", kComponentNames[i], axisColors[i], values[i], speed, result);
        const bool labelHovered = ImGui::IsItemHovered();
        ImGui::SameLine(0.0f, 0.0f);
        floatField("##value", values[i], fieldWidth, "%.2f", result);
        if (rightReleased && (labelHovered || ImGui::IsItemHovered())) {
            ImGui::GetStateStorage()->SetInt(clickedKey, i);
        }
        ImGui::PopID();
    }
}

// Имя слоя, а у слоя без имени - номер.
std::string layerLabel(int layer, std::span<const std::string> names)
{
    const auto index = static_cast<std::size_t>(layer);

    return index < names.size() && !names[index].empty() ? names[index] : std::to_string(layer);
}

// Слой без имени не используется и в списках не показывается, если он не выбран
// сейчас. Без проекта имён нет вовсе, тогда видны все.
bool layerListed(int layer, std::span<const std::string> names, bool selected)
{
    const auto index = static_cast<std::size_t>(layer);

    return selected || names.empty() || (index < names.size() && !names[index].empty());
}

// Последний пункт списков слоёв. Вызывается внутри открытого списка.
void editLayersItem(const PropertyEditorContext& context, bool& requested)
{
    if (!context.editLayers) {
        return;
    }
    ImGui::Separator();
    if (ImGui::Selectable(tr("editor.layers.edit").c_str())) {
        requested = true;
    }
}

bool layerCombo(int& layer, const PropertyEditorContext& context)
{
    bool changed = false;
    bool editRequested = false;
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("##value", layerLabel(layer, context.layerNames).c_str())) {
        for (int i = 0; i < kLayerCount; ++i) {
            if (!layerListed(i, context.layerNames, i == layer)) {
                continue;
            }
            if (ImGui::Selectable(layerLabel(i, context.layerNames).c_str(), i == layer)) {
                layer = i;
                changed = true;
            }
        }
        editLayersItem(context, editRequested);
        ImGui::EndCombo();
    }
    if (editRequested) {
        context.editLayers();
    }

    return changed;
}

std::string maskPreview(LayerMask mask, std::span<const std::string> names)
{
    if (mask == kAllLayers) {
        return tr("editor.layers.everything");
    }
    if (mask == 0) {
        return tr("editor.layers.nothing");
    }
    std::string result;
    for (int i = 0; i < kLayerCount; ++i) {
        if (inMask(mask, i)) {
            result += (result.empty() ? "" : ", ") + layerLabel(i, names);
        }
    }

    return result;
}

// Маска хранится в int с теми же битами, см. PropertyStorage<std::uint32_t>.
bool layerMaskCombo(int& stored, const PropertyEditorContext& context)
{
    auto mask = std::bit_cast<LayerMask>(stored);
    bool changed = false;
    bool editRequested = false;
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("##value", maskPreview(mask, context.layerNames).c_str())) {
        if (ImGui::Selectable(tr("editor.layers.everything").c_str(), mask == kAllLayers)) {
            mask = kAllLayers;
            changed = true;
        }
        if (ImGui::Selectable(tr("editor.layers.nothing").c_str(), mask == 0)) {
            mask = 0;
            changed = true;
        }
        ImGui::Separator();
        for (int i = 0; i < kLayerCount; ++i) {
            bool on = inMask(mask, i);
            if (!layerListed(i, context.layerNames, on)) {
                continue;
            }
            ImGui::PushID(i);
            if (checkbox(layerLabel(i, context.layerNames).c_str(), &on, context.metrics.checkboxPadding)) {
                mask ^= LayerMask{1} << i;
                changed = true;
            }
            ImGui::PopID();
        }
        editLayersItem(context, editRequested);
        ImGui::EndCombo();
    }
    stored = std::bit_cast<int>(mask);
    if (editRequested) {
        context.editLayers();
    }

    return changed;
}

// Имена слоёв по номеру: число строк постоянное, слой 0 не переименовывается.
bool layerNamesField(ListValue& list)
{
    bool changed = false;
    for (std::size_t i = 0; i < list.items.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%2zu", i);
        ImGui::SameLine();
        ImGui::BeginDisabled(i == 0);
        ImGui::SetNextItemWidth(-FLT_MIN);
        changed = ImGui::InputText("##name", &std::get<std::string>(list.items[i])) || changed;
        ImGui::EndDisabled();
        ImGui::PopID();
    }

    return changed;
}

bool enumCombo(const Property& property, EnumValue& value)
{
    const auto& names = property.enumNames();
    const auto current = static_cast<std::size_t>(value.value);
    const std::string preview = value.value >= 0 && current < names.size() ? names[current]
                                                                           : std::to_string(value.value);

    bool changed = false;
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("##value", preview.c_str())) {
        for (std::size_t i = 0; i < names.size(); ++i) {
            if (ImGui::Selectable(names[i].c_str(), i == current)) {
                value.value = static_cast<int>(i);
                changed = true;
            }
        }
        ImGui::EndCombo();
    }

    return changed;
}

void collectNodes(const Node& node, std::vector<const Node*>& out)
{
    out.push_back(&node);
    for (const auto& child : node.children()) {
        collectNodes(*child, out);
    }
}

// Узел сцены по имени. Удалённый узел показывается как "нет".
bool nodeCombo(const Scene& scene, NodeRef& value)
{
    std::vector<const Node*> nodes;
    if (const Node* root = scene.root()) {
        collectNodes(*root, nodes);
    }
    const auto current = std::ranges::find(nodes, value.id, &Node::id);
    const std::string none = tr("editor.inspector.none");
    const char* preview = current != nodes.end() ? (*current)->name().c_str() : none.c_str();

    bool changed = false;
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("##value", preview)) {
        if (ImGui::Selectable(none.c_str(), value.id == 0)) {
            value.id = 0;
            changed = true;
        }
        for (const Node* node : nodes) {
            ImGui::PushID(node);
            if (ImGui::Selectable(node->name().c_str(), node->id() == value.id)) {
                value.id = node->id();
                changed = true;
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }

    return changed;
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
            dragSpeed(property),
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

// Типы компонентов, у которых есть порядок выполнения: только те, что
// переопределяют фазы кадра. Выбранный виден всегда, даже если его модуль не загружен.
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

// Первый орган, заданный в этом кадре нажатием (см. wasControlActivated);
// Escape не считается: им отменяют ожидание.
std::optional<InputControl> activatedControl(const Input& input)
{
    for (const InputControl& control : allInputControls()) {
        if (control != InputControl{Key::Escape} && wasControlActivated(input, control)) {
            return control;
        }
    }

    return std::nullopt;
}

// Привязка действия: кнопка с именем органа управления и список всех органов.
// Нажали кнопку - она ждёт следующего нажатия клавиши, кнопки мыши или
// геймпада или отклонения стика и запоминает его; Escape отменяет. Оси мыши
// нажатием не задать - их выбирают из списка. Какое поле ждёт, хранится в
// памяти окна ImGui.
bool inputControlField(std::string& value, const Input& input)
{
    ImGuiStorage& storage = *ImGui::GetStateStorage();
    const ImGuiID waitingKey = ImGui::GetID(kWaitingForControl);
    const bool waiting = storage.GetBool(waitingKey, false);
    bool changed = false;

    const float listWidth = ImGui::GetFrameHeight();
    std::string label = waiting         ? tr("editor.inspector.pressControl")
                        : value.empty() ? tr("editor.inspector.none")
                                        : value;
    label += "##control";
    if (ImGui::Button(label.c_str(), ImVec2(ImGui::GetContentRegionAvail().x - listWidth, 0.0f))) {
        storage.SetBool(waitingKey, !waiting);
    }
    ImGui::SameLine(0.0f, 0.0f);
    ImGui::SetNextItemWidth(listWidth);
    if (ImGui::BeginCombo("##controls", nullptr, ImGuiComboFlags_NoPreview | ImGuiComboFlags_PopupAlignLeft)) {
        for (const InputControl& control : allInputControls()) {
            const std::string name(controlName(control));
            if (ImGui::Selectable(name.c_str(), name == value)) {
                value = name;
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    if (!waiting) {
        return changed;
    }
    if (input.wasKeyPressed(Key::Escape)) {
        storage.SetBool(waitingKey, false);

        return changed;
    }
    if (const auto activated = activatedControl(input)) {
        storage.SetBool(waitingKey, false);
        value = std::string(controlName(*activated));
        changed = true;
    }

    return changed;
}

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
}

void valueField(
    const Property& property,
    PropertyType type,
    PropertyValue& value,
    const PropertyEditorContext& context,
    PropertyResult& result
)
{
    const float speed = dragSpeed(property);
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
        result.changed = nodeCombo(context.scene, std::get<NodeRef>(value)) || result.changed;
        break;
    case PropertyType::Resource:
        if (const std::string& kind = property.attributes().resourceKind; !kind.empty()) {
            result.changed = resourceField(kind, std::get<ResourceRef>(value), context) || result.changed;
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
    const std::function<std::optional<PropertyValue>()>& defaults
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
    ImGui::EndPopup();
}

template <typename Object>
PropertyResult drawPropertyRow(const Property& property, Object& object, const PropertyEditorContext& context)
{
    PropertyResult result;
    PropertyValue value = property.get(object);

    ImGui::PushID(property.name().c_str());
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    const float rowTop = ImGui::GetCursorScreenPos().y;
    rowLabel(property, value, result);
    ImGui::TableNextColumn();
    valueField(property, property.type(), value, context, result);
    const float rowBottom = ImGui::GetItemRectMax().y;
    rowMenu(property, value, rowTop, rowBottom, result, [&] {
        return defaultValue(context.registry, object, property);
    });
    // Подпись числа тянут мимо поля значения: пределы и для неё.
    if (result.changed) {
        clampToRange(property, value);
    }
    ImGui::PopID();

    result.value = std::move(value);

    return result;
}

} // namespace

PropertyResult drawProperty(const Property& property, Node& node, const PropertyEditorContext& context)
{
    return drawPropertyRow(property, node, context);
}

PropertyResult drawProperty(const Property& property, Component& component, const PropertyEditorContext& context)
{
    return drawPropertyRow(property, component, context);
}

PropertyResult drawProperty(const Property& property, SettingsSection& section, const PropertyEditorContext& context)
{
    return drawPropertyRow(property, section, context);
}

std::string propertyLabel(const Property& property)
{
    if (!property.attributes().displayName.empty()) {
        return property.attributes().displayName;
    }
    const std::string& name = property.name();
    const std::string key = "property." + name;
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
