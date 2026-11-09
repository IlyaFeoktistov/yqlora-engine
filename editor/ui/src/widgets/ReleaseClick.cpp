#include "yqlora/editor/ui/widgets/ReleaseClick.hpp"

#include <imgui.h>
#include <imgui_internal.h>

namespace yqlora::editor {

bool ReleaseClick::released(std::uint64_t key)
{
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen()) {
        m_pressed = key;
    }
    if (m_pressed != key || !ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        return false;
    }
    m_pressed = 0;

    // Путь мыши с нажатия до отпускания: дальше порога - это было перетаскивание.
    return ImGui::IsItemHovered() && !ImGui::IsMouseDragPastThreshold(ImGuiMouseButton_Left);
}

} // namespace yqlora::editor
