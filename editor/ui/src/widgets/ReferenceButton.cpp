#include "yqlora/editor/ui/widgets/ReferenceButton.hpp"

#include "yqlora/editor/ui/style/Icons.hpp"

#include "yqlora/core/Text.hpp"
#include "yqlora/i18n/Translate.hpp"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>

namespace yqlora::editor {

namespace {

// Высота списка выбора в строках поля.
constexpr float kChoiceListLines = 12.0f;

} // namespace

ReferenceClick referenceButton(const std::string& label, const std::string& tooltip)
{
    ImGui::BeginGroup();
    const float pickWidth = ImGui::GetFrameHeight();
    const float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
    const float fieldWidth = std::max(ImGui::GetContentRegionAvail().x - pickWidth - spacing, pickWidth);
    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered));
    const bool field = ImGui::Button((label + "##field").c_str(), ImVec2(fieldWidth, 0.0f));
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();
    ImGui::SetItemTooltip("%s", tooltip.c_str());
    ImGui::SameLine(0.0f, spacing);
    const bool pick = ImGui::Button(icons::kPick, ImVec2(pickWidth, 0.0f));
    ImGui::SetItemTooltip("%s", tr("editor.resourceField.pick").c_str());
    ImGui::EndGroup();
    if (pick) {
        return ReferenceClick::Pick;
    }

    return field ? ReferenceClick::Field : ReferenceClick::None;
}

int drawChoiceList(std::span<const std::string> labels, int current, std::string& search)
{
    if (ImGui::IsWindowAppearing()) {
        search.clear();
        ImGui::SetKeyboardFocusHere();
    }
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##search", tr("editor.resourceField.search").c_str(), &search);

    int chosen = ChoiceList::kNothing;
    if (ImGui::BeginChild("##choices", ImVec2(0.0f, ImGui::GetFrameHeight() * kChoiceListLines))) {
        if (search.empty() && ImGui::Selectable(tr("editor.inspector.none").c_str(), current == ChoiceList::kNone)) {
            chosen = ChoiceList::kNone;
        }
        bool any = false;
        for (int i = 0; i < static_cast<int>(labels.size()); ++i) {
            const std::string& label = labels[static_cast<std::size_t>(i)];
            if (!containsIgnoringCase(label, search)) {
                continue;
            }
            any = true;
            ImGui::PushID(i);
            if (ImGui::Selectable(label.c_str(), i == current)) {
                chosen = i;
            }
            ImGui::PopID();
        }
        if (!any && !search.empty()) {
            ImGui::TextDisabled("%s", tr("editor.resourceField.nothing").c_str());
        }
    }
    ImGui::EndChild();
    if (chosen != ChoiceList::kNothing) {
        ImGui::CloseCurrentPopup();
    }

    return chosen;
}

} // namespace yqlora::editor
