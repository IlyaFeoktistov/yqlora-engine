#include "yqlora/editor/ui/widgets/NameDialog.hpp"

#include "yqlora/editor/ui/style/Theme.hpp"

#include "yqlora/i18n/Translate.hpp"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <utility>

namespace yqlora::editor {

namespace {

constexpr float kNameFieldWidth = 360.0f;

} // namespace

void NameDialog::open(std::string name)
{
    m_name = std::move(name);
    m_failed = false;
    m_requested = true;
}

void NameDialog::draw(
    const Text& text,
    const std::function<bool(std::string_view)>& valid,
    const std::function<bool(const std::string&)>& confirm,
    const EditorColors& colors
)
{
    if (std::exchange(m_requested, false)) {
        ImGui::OpenPopup(text.title.c_str());
    }
    if (!ImGui::BeginPopupModal(text.title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }
    if (!text.label.empty()) {
        ImGui::TextUnformatted(text.label.c_str());
    }
    if (ImGui::IsWindowAppearing()) {
        ImGui::SetKeyboardFocusHere();
    }
    ImGui::SetNextItemWidth(kNameFieldWidth);
    const bool entered = ImGui::InputText("##name", &m_name, ImGuiInputTextFlags_EnterReturnsTrue);
    if (ImGui::IsItemEdited()) {
        m_failed = false;
    }
    const bool isValid = valid(m_name);
    if (!isValid || m_failed) {
        ImGui::TextColored(toImVec4(colors.errorText), "%s", (isValid ? text.failed : text.invalid).c_str());
    }

    ImGui::BeginDisabled(!isValid);
    if (ImGui::Button(text.confirm.c_str()) || (entered && isValid)) {
        if (confirm(m_name)) {
            ImGui::CloseCurrentPopup();
        } else {
            m_failed = true;
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button(tr("editor.common.cancel").c_str()) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

} // namespace yqlora::editor
