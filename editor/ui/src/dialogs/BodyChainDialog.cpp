#include "yqlora/editor/ui/dialogs/BodyChainDialog.hpp"

#include "yqlora/editor/ui/widgets/WorldPointField.hpp"

#include "yqlora/editor/EditorSession.hpp"
#include "yqlora/i18n/Translate.hpp"
#include "yqlora/physics/PhysicsChain.hpp"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <string>
#include <utility>

namespace yqlora::editor {

namespace {

constexpr ImVec2 kWindowSize(440.0f, 0.0f);
constexpr float kDragSpeed = 0.01f;
constexpr float kMaxChainMass = 10000.0f;
constexpr float kMaxChainSize = 100.0f;

} // namespace

void BodyChainDialog::draw(EditorSession& session)
{
    const std::string title = tr("editor.bodyChain.title") + "###BodyChain";
    // Цепь тел лежит от своего места вдоль +Z мира.
    const auto showPath = [&] {
        session.pointPick().showGuide({m_settings.position, m_settings.position + Vec3(0.0f, 0.0f, m_settings.length)});
    };
    if (m_points.picking(session)) {
        showPath();

        return;
    }
    if (std::exchange(m_requested, false) || m_points.takeReopen()) {
        ImGui::OpenPopup(title.c_str());
    }
    ImGui::SetNextWindowSize(kWindowSize, ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal(title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }
    showPath();

    ImGui::InputText(tr("editor.bodyChain.name").c_str(), &m_settings.name);
    m_points.field("editor.bodyChain.position", m_settings.position, session);
    ImGui::DragInt(
        tr("editor.bodyChain.segments").c_str(),
        &m_settings.segments,
        1.0f,
        kMinChainSegments,
        kMaxChainSegments
    );
    ImGui::DragFloat(
        tr("editor.bodyChain.length").c_str(),
        &m_settings.length,
        kDragSpeed,
        kMinChainSize,
        kMaxChainSize
    );
    ImGui::DragFloat(
        tr("editor.bodyChain.radius").c_str(),
        &m_settings.radius,
        kDragSpeed,
        kMinChainSize,
        kMaxChainSize
    );
    ImGui::DragFloat(tr("editor.bodyChain.mass").c_str(), &m_settings.mass, kDragSpeed, kMinChainSize, kMaxChainMass);
    ImGui::TextDisabled("%s", tr("editor.bodyChain.hint").c_str());

    ImGui::BeginDisabled(m_settings.name.empty());
    if (ImGui::Button(tr("editor.bodyChain.create").c_str()) && createBodyChain(session, m_settings)) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button(tr("editor.common.cancel").c_str()) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

} // namespace yqlora::editor
