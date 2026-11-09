#include "yqlora/editor/ui/widgets/PingFlash.hpp"

#include <imgui.h>

namespace yqlora::editor {

bool drawPingFlash(double startedAt)
{
    const double elapsed = ImGui::GetTime() - startedAt;
    if (elapsed >= kPingSeconds) {
        return false;
    }
    ImVec4 color = ImGui::GetStyleColorVec4(ImGuiCol_DragDropTarget);
    color.w *= static_cast<float>(1.0 - elapsed / kPingSeconds);
    ImGui::GetWindowDrawList()
        ->AddRectFilled(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImGui::GetColorU32(color));

    return true;
}

} // namespace yqlora::editor
