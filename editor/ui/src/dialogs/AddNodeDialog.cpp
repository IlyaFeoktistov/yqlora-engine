#include "yqlora/editor/ui/dialogs/AddNodeDialog.hpp"

#include "yqlora/editor/ui/style/Icons.hpp"
#include "yqlora/editor/ui/style/NodeIcons.hpp"

#include "yqlora/core/Text.hpp"
#include "yqlora/editor/EditorSession.hpp"
#include "yqlora/i18n/Translate.hpp"
#include "yqlora/reflection/TypeRegistry.hpp"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>

namespace yqlora::editor {

namespace {

constexpr ImVec2 kWindowSize(420.0f, 460.0f);
// Под списком типов: описание выбранного типа и кнопки.
constexpr float kFooterRows = 4.0f;

bool isCreatableNode(const TypeInfo& type)
{
    return type.kind() == TypeKind::Node && type.creatable();
}

} // namespace

void AddNodeDialog::open(NodeId parent)
{
    m_parent = parent;
    m_search.clear();
    m_requested = true;
}

void AddNodeDialog::draw(EditorSession& session, const TypeRegistry& registry)
{
    const std::string title = tr("editor.addNode.title") + "###AddNode";
    if (m_requested) {
        ImGui::OpenPopup(title.c_str());
        m_requested = false;
    }
    ImGui::SetNextWindowSize(kWindowSize, ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal(title.c_str())) {
        return;
    }

    if (ImGui::IsWindowAppearing()) {
        ImGui::SetKeyboardFocusHere();
    }
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##search", tr("editor.addNode.search").c_str(), &m_search);

    bool create = false;
    const float footer = ImGui::GetFrameHeightWithSpacing() * kFooterRows;
    if (ImGui::BeginChild("types", ImVec2(0.0f, -footer), ImGuiChildFlags_Borders)) {
        for (const auto& type : registry.types()) {
            // При поиске - плоский список совпадений, без поиска - дерево от корневых типов.
            const bool show = m_search.empty() ? type->kind() == TypeKind::Node && type->baseName().empty()
                                               : isCreatableNode(*type) && matchesSearch(*type);
            if (show) {
                create = drawType(registry, *type) || create;
            }
        }
    }
    ImGui::EndChild();

    const TypeInfo* selected = registry.find(m_selectedType);
    if (selected) {
        ImGui::TextWrapped("%s", tr(selected->descriptionKey()).c_str());
    }

    ImGui::SetCursorPosY(
        ImGui::GetWindowHeight() - ImGui::GetFrameHeightWithSpacing() - ImGui::GetStyle().WindowPadding.y
    );
    ImGui::BeginDisabled(!selected || !isCreatableNode(*selected));
    create = ImGui::Button(tr("editor.addNode.create").c_str()) || create;
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button(tr("editor.common.cancel").c_str()) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        ImGui::CloseCurrentPopup();
    }

    if (create && selected && isCreatableNode(*selected)) {
        session.editing().createNode(*selected, session.findNode(m_parent));
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

bool AddNodeDialog::drawType(const TypeRegistry& registry, const TypeInfo& type)
{
    bool hasChildren = false;
    if (m_search.empty()) {
        for (const auto& other : registry.types()) {
            hasChildren = hasChildren || (other->baseName() == type.name() && other->kind() == TypeKind::Node);
        }
    }

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow
                               | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (!hasChildren) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }
    if (type.name() == m_selectedType) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    const bool
        open = ImGui::TreeNodeEx(type.name().c_str(), flags, "%s  %s", nodeIcon(type.name()), type.name().c_str());
    bool chosen = false;
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
        m_selectedType = type.name();
        chosen = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
    }
    ImGui::SetItemTooltip("%s", tr(type.descriptionKey()).c_str());

    if (open && hasChildren) {
        for (const auto& other : registry.types()) {
            if (other->baseName() == type.name() && other->kind() == TypeKind::Node) {
                chosen = drawType(registry, *other) || chosen;
            }
        }
        ImGui::TreePop();
    }

    return chosen;
}

bool AddNodeDialog::matchesSearch(const TypeInfo& type) const
{
    return containsIgnoringCase(type.name(), m_search);
}

} // namespace yqlora::editor
