#include "yqlora/editor/ui/widgets/IconButton.hpp"

#include <imgui.h>

namespace yqlora::editor {

bool iconButton(const char* icon, bool on)
{
    if (on) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    }
    const bool clicked = ImGui::Button(icon);
    // Цвет снимается по тому же on, с которым положен: щелчок мог сменить
    // состояние у вызывающего.
    if (on) {
        ImGui::PopStyleColor();
    }

    return clicked;
}

bool iconToggle(const char* icon, bool& value)
{
    if (!iconButton(icon, value)) {
        return false;
    }
    value = !value;

    return true;
}

float iconButtonWidth(const char* icon)
{
    return ImGui::CalcTextSize(icon).x + ImGui::GetStyle().FramePadding.x * 2.0f;
}

} // namespace yqlora::editor
