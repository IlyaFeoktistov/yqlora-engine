#include "yqlora/editor/ui/widgets/HelpTooltip.hpp"

#include <imgui.h>

namespace yqlora::editor {

namespace {

// Ширина строки описания в высотах шрифта: около 60 знаков.
constexpr float kHelpWidthInFontSizes = 28.0f;

} // namespace

void helpTooltip(const std::string& text)
{
    if (text.empty() || !ImGui::BeginItemTooltip()) {
        return;
    }
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * kHelpWidthInFontSizes);
    ImGui::TextUnformatted(text.c_str());
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
}

} // namespace yqlora::editor
