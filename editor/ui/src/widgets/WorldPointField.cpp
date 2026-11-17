#include "yqlora/editor/ui/widgets/WorldPointField.hpp"

#include "yqlora/editor/EditorSession.hpp"
#include "yqlora/i18n/Translate.hpp"
#include "yqlora/scene/Node3D.hpp"

#include <imgui.h>

namespace yqlora::editor {

namespace {

constexpr float kDragSpeed = 0.01f;

} // namespace

void worldPointField(const char* labelKey, Vec3& point, EditorSession& session)
{
    ImGui::PushID(labelKey);
    const auto* selected = dynamic_cast<const Node3D*>(session.selectedNode());
    ImGui::BeginDisabled(selected == nullptr);
    if (ImGui::Button(tr("editor.chain.fromSelected").c_str()) && selected) {
        point = selected->worldPosition();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::DragFloat3(tr(labelKey).c_str(), &point.x, kDragSpeed);
    ImGui::PopID();
}

} // namespace yqlora::editor
