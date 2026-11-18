#include "yqlora/editor/ui/widgets/WorldPointField.hpp"

#include "yqlora/editor/EditorSession.hpp"
#include "yqlora/i18n/Translate.hpp"
#include "yqlora/scene/Node3D.hpp"

#include <imgui.h>

#include <utility>

namespace yqlora::editor {

namespace {

constexpr float kDragSpeed = 0.01f;

} // namespace

bool WorldPointFields::picking(EditorSession& session)
{
    if (!m_target) {
        return false;
    }
    PointPick& pick = session.pointPick();
    if (pick.waitingFor(m_target)) {
        return true;
    }
    if (const auto point = pick.take(m_target)) {
        *m_target = *point;
    }
    m_target = nullptr;
    m_reopen = true;

    return false;
}

bool WorldPointFields::takeReopen()
{
    return std::exchange(m_reopen, false);
}

void WorldPointFields::field(const char* labelKey, Vec3& point, EditorSession& session)
{
    ImGui::PushID(labelKey);
    const auto* selected = dynamic_cast<const Node3D*>(session.selectedNode());
    ImGui::BeginDisabled(selected == nullptr);
    if (ImGui::Button(tr("editor.chain.fromSelected").c_str()) && selected) {
        point = selected->worldPosition();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button(tr("editor.chain.pick").c_str())) {
        session.pointPick().request(&point, session.scene());
        m_target = &point;
        ImGui::CloseCurrentPopup();
    }
    ImGui::SetItemTooltip("%s", tr("editor.chain.pickTooltip").c_str());
    ImGui::SameLine();
    ImGui::DragFloat3(tr(labelKey).c_str(), &point.x, kDragSpeed);
    ImGui::PopID();
}

} // namespace yqlora::editor
