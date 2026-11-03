#include "yqlora/editor/ui/widgets/NumberFields.hpp"

#include "yqlora/editor/ui/style/Theme.hpp"

#include <imgui.h>

namespace yqlora::editor {

namespace {

constexpr float kFastFactor = 10.0f; // с Shift
constexpr float kSlowFactor = 0.1f;  // с Ctrl

} // namespace

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

void floatField(const char* id, float& value, float width, const char* format, PropertyResult& result)
{
    ImGui::SetNextItemWidth(width);
    if (ImGui::InputFloat(id, &value, 0.0f, 0.0f, format)) {
        result.changed = true;
    }
}

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

} // namespace yqlora::editor
