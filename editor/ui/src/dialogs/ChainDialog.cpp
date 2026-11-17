#include "yqlora/editor/ui/dialogs/ChainDialog.hpp"

#include "yqlora/editor/ui/widgets/WorldPointField.hpp"

#include "yqlora/editor/EditorSession.hpp"
#include "yqlora/i18n/Translate.hpp"
#include "yqlora/reflection/EnumNames.hpp"

#include <imgui.h>

#include <iterator>
#include <string>

namespace yqlora::editor {

namespace {

constexpr ImVec2 kWindowSize(440.0f, 0.0f);
constexpr float kDragSpeed = 0.01f;
constexpr float kMaxLinkMass = 1000.0f;
constexpr float kMaxLinkSize = 10.0f;

} // namespace

void ChainDialog::draw(EditorSession& session)
{
    const std::string title = tr("editor.chain.title") + "###Chain";
    if (m_requested) {
        ImGui::OpenPopup(title.c_str());
        m_requested = false;
    }
    ImGui::SetNextWindowSize(kWindowSize, ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal(title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    worldPointField("editor.chain.start", m_settings.start, session);
    worldPointField("editor.chain.end", m_settings.end, session);
    ImGui::DragFloat(
        tr("editor.chain.spacing").c_str(),
        &m_settings.spacing,
        kDragSpeed,
        kMinChainLinkSize,
        kMaxLinkSize
    );
    ImGui::DragFloat(
        tr("editor.chain.radius").c_str(),
        &m_settings.radius,
        kDragSpeed,
        kMinChainLinkSize,
        kMaxLinkSize
    );
    ImGui::DragFloat(tr("editor.chain.linkMass").c_str(), &m_settings.linkMass, kDragSpeed, kDragSpeed, kMaxLinkMass);
    const auto& names = EnumNames<ChainLinkJoint>::names;
    const auto current = static_cast<std::size_t>(m_settings.joint);
    if (ImGui::BeginCombo(tr("editor.chain.joint").c_str(), tr("enum." + std::string(names[current])).c_str())) {
        for (std::size_t i = 0; i < std::size(names); ++i) {
            if (ImGui::Selectable(tr("enum." + std::string(names[i])).c_str(), i == current)) {
                m_settings.joint = static_cast<ChainLinkJoint>(i);
            }
        }
        ImGui::EndCombo();
    }
    ImGui::Checkbox(tr("editor.chain.attachStart").c_str(), &m_settings.attachStart);
    ImGui::Checkbox(tr("editor.chain.attachEnd").c_str(), &m_settings.attachEnd);
    ImGui::TextDisabled("%s", tr("editor.chain.hint").c_str());

    if (ImGui::Button(tr("editor.chain.create").c_str())) {
        if (createChain(session, m_settings)) {
            ImGui::CloseCurrentPopup();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(tr("editor.common.cancel").c_str()) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

} // namespace yqlora::editor
