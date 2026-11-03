#include "yqlora/editor/ui/inspectors/ResourceKindInspector.hpp"

#include "yqlora/i18n/Translate.hpp"

#include <imgui.h>

namespace yqlora::editor {

ImportAction importButtons(bool changed)
{
    ImportAction action = ImportAction::None;
    ImGui::BeginDisabled(!changed);
    if (ImGui::Button(tr("editor.resource.apply").c_str())) {
        action = ImportAction::Apply;
    }
    ImGui::SameLine();
    if (ImGui::Button(tr("editor.resource.revert").c_str())) {
        action = ImportAction::Revert;
    }
    ImGui::EndDisabled();

    return action;
}

} // namespace yqlora::editor
