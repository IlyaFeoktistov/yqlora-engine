#include "yqlora/editor/ui/dialogs/TypePickerDialog.hpp"

#include "yqlora/editor/ui/core/EditorContext.hpp"
#include "yqlora/editor/ui/core/EditorHost.hpp"
#include "yqlora/editor/ui/style/Icons.hpp"
#include "yqlora/editor/ui/style/Theme.hpp"
#include "yqlora/editor/ui/style/TypeIcons.hpp"

#include "yqlora/editor/EditorSession.hpp"
#include "yqlora/editor/EditorSettings.hpp"
#include "yqlora/editor/GuideLink.hpp"
#include "yqlora/editor/TypeTexts.hpp"
#include "yqlora/i18n/Language.hpp"
#include "yqlora/i18n/Translate.hpp"
#include "yqlora/platform/Platform.hpp"
#include "yqlora/project/Project.hpp"
#include "yqlora/reflection/TypeRegistry.hpp"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>

namespace yqlora::editor {

namespace {

constexpr ImVec2 kWindowSize(760.0f, 560.0f);
// Ширина колонки полок в высотах шрифта.
constexpr float kShelvesWidthInFontSizes = 12.0f;
// Высота подробностей выбранного типа в строках.
constexpr float kDetailsLines = 6.0f;
// Отступ строки списка и её вложенных полок, в высотах шрифта.
constexpr float kIndentInFontSizes = 1.0f;
// Строка типа - две строки текста: имя и описание (или причина).
constexpr int kRowTextLines = 2;

std::string firstLine(const std::string& text)
{
    return text.substr(0, text.find('\n'));
}

} // namespace

void TypePickerDialog::openComponents(NodeId node)
{
    open(Mode::Components, node);
}

void TypePickerDialog::openNodes(NodeId parent)
{
    open(Mode::Nodes, parent);
}

void TypePickerDialog::open(Mode mode, NodeId target)
{
    m_mode = mode;
    m_target = target;
    m_search.clear();
    m_catalog.reset();
    m_shelf = 0;
    m_requested = true;
}

bool TypePickerDialog::rebuild(EditorContext& context)
{
    EditorSession& session = context.session;
    const Localization& english = context.host.language().english();
    m_revision = context.registry.revision();
    if (m_mode == Mode::Nodes) {
        m_catalog = TypeCatalog::nodes(context.registry, &english);
        keepShelfInRange();

        return true;
    }
    const Node* node = session.findNode(m_target);
    if (!node) {
        return false;
    }
    const std::filesystem::path project = session.project() ? session.project()->directory() : std::filesystem::path();
    m_catalog = TypeCatalog::components(context.registry, *node, project, &english);
    keepShelfInRange();

    return true;
}

void TypePickerDialog::keepShelfInRange()
{
    // После перезагрузки модуля игры полок скриптов может стать меньше.
    if (m_shelf >= m_catalog->shelves().size()) {
        m_shelf = 0;
    }
}

void TypePickerDialog::draw(EditorContext& context)
{
    const std::string title = tr(m_mode == Mode::Nodes ? "editor.typePicker.nodeTitle"
                                                       : "editor.typePicker.componentTitle")
                              + "###TypePicker";
    if (m_requested) {
        m_requested = false;
        if (rebuild(context)) {
            ImGui::OpenPopup(title.c_str());
        }
    }
    ImGui::SetNextWindowSize(kWindowSize, ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal(title.c_str())) {
        return;
    }
    // Модуль игры перезагрузили или узел пропал, пока окно открыто.
    if ((!m_catalog || m_revision != context.registry.revision()) && !rebuild(context)) {
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();

        return;
    }

    // Набранное сразу идёт в поиск: поле в фокусе, пока окно открыто.
    if (!ImGui::IsAnyItemActive()) {
        ImGui::SetKeyboardFocusHere();
    }
    ImGui::SetNextItemWidth(-FLT_MIN);
    const std::string hint = std::string(icons::kSearch) + " " + tr("editor.typePicker.search");
    ImGui::InputTextWithHint("##search", hint.c_str(), &m_search);

    std::vector<std::string>* recent = context.session.project()
                                           ? &context.settings.recentTypesOf(context.session.project()->directory())
                                           : nullptr;
    const std::vector<std::string> noRecent;
    const std::vector<const TypeEntry*> entries = m_catalog->list(
        m_catalog->shelves()[m_shelf],
        m_search,
        recent ? *recent : noRecent
    );
    m_selected = keptSelection(entries, m_selected);
    const int step = (ImGui::IsKeyPressed(ImGuiKey_DownArrow) ? 1 : 0)
                     - (ImGui::IsKeyPressed(ImGuiKey_UpArrow) ? 1 : 0);
    if (step != 0) {
        m_selected = steppedSelection(entries, m_selected, step);
        m_scrollToSelected = true;
    }

    const float footer = ImGui::GetTextLineHeightWithSpacing() * kDetailsLines + ImGui::GetFrameHeightWithSpacing();
    bool chosen = false;
    if (ImGui::BeginChild(
            "shelves",
            ImVec2(ImGui::GetFontSize() * kShelvesWidthInFontSizes, -footer),
            ImGuiChildFlags_Borders
        )) {
        drawShelves();
    }
    ImGui::EndChild();
    ImGui::SameLine();
    if (ImGui::BeginChild("types", ImVec2(0.0f, -footer), ImGuiChildFlags_Borders)) {
        chosen = drawList(context, entries);
    }
    ImGui::EndChild();

    const TypeEntry* selected = m_catalog->find(m_selected);
    drawDetails(context, selected);

    const bool fits = selected && selected->fit.problem == AttachProblem::None;
    ImGui::SetCursorPosY(
        ImGui::GetWindowHeight() - ImGui::GetFrameHeightWithSpacing() - ImGui::GetStyle().WindowPadding.y
    );
    ImGui::BeginDisabled(!fits);
    chosen = ImGui::Button(tr("editor.typePicker.add").c_str()) || chosen;
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button(tr("editor.common.cancel").c_str()) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        ImGui::CloseCurrentPopup();
    }
    chosen = chosen || ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter);
    if (chosen && fits) {
        add(context, *selected);
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void TypePickerDialog::drawShelves()
{
    const auto shelves = m_catalog->shelves();
    for (std::size_t i = 0; i < shelves.size(); ++i) {
        const TypeShelf& shelf = shelves[i];
        ImGui::PushID(static_cast<int>(i));
        const float indent = static_cast<float>(shelf.depth) * ImGui::GetFontSize() * kIndentInFontSizes;
        if (indent > 0.0f) {
            ImGui::Indent(indent);
        }
        // Поиск идёт по всем полкам: выбранная полка ему не важна.
        ImGui::BeginDisabled(!m_search.empty());
        if (ImGui::Selectable(shelf.label.c_str(), i == m_shelf)) {
            m_shelf = i;
        }
        ImGui::EndDisabled();
        if (indent > 0.0f) {
            ImGui::Unindent(indent);
        }
        ImGui::PopID();
    }
}

bool TypePickerDialog::drawList(EditorContext& context, const std::vector<const TypeEntry*>& entries)
{
    const EditorColors& colors = context.theme.colors();
    const float lineHeight = ImGui::GetTextLineHeight();
    const float rowHeight = lineHeight * kRowTextLines + ImGui::GetStyle().ItemSpacing.y;
    const ImU32 textColor = ImGui::GetColorU32(ImGuiCol_Text);
    const ImU32 dimColor = ImGui::GetColorU32(ImGuiCol_TextDisabled);
    const ImU32 warningColor = toImU32(colors.warningText);
    if (entries.empty()) {
        ImGui::TextDisabled("%s", tr("editor.typePicker.noTypes").c_str());

        return false;
    }

    bool chosen = false;
    for (const TypeEntry* entry : entries) {
        const TypeInfo& type = *entry->type;
        ImGui::PushID(type.name().c_str());
        const bool selected = type.name() == m_selected;
        const ImVec2 position = ImGui::GetCursorScreenPos();
        if (ImGui::Selectable("##row", selected, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(0.0f, rowHeight))) {
            m_selected = type.name();
            chosen = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
        }
        if (selected && m_scrollToSelected) {
            ImGui::SetScrollHereY();
            m_scrollToSelected = false;
        }
        const bool fits = entry->fit.problem == AttachProblem::None;
        const ImVec2 rowMax(ImGui::GetItemRectMax());
        ImDrawList& draw = *ImGui::GetWindowDrawList();
        draw.PushClipRect(position, rowMax, true);
        // Имя на языке человека и имя типа, если оно другое: его видно в инспекторе и коде.
        std::string name = std::string(typeIcon(context.registry, type)) + "  " + entry->name;
        draw.AddText(position, fits ? textColor : dimColor, name.c_str());
        float right = position.x + ImGui::CalcTextSize(name.c_str()).x;
        if (entry->name != type.name()) {
            const std::string typeName = "  " + type.name();
            draw.AddText(ImVec2(right, position.y), dimColor, typeName.c_str());
            right += ImGui::CalcTextSize(typeName.c_str()).x;
        }
        // В результатах поиска - на какой полке тип, справа.
        if (!m_search.empty() && !entry->shelfLabel.empty()) {
            const float width = ImGui::CalcTextSize(entry->shelfLabel.c_str()).x;
            draw.AddText(ImVec2(std::max(right, rowMax.x - width), position.y), dimColor, entry->shelfLabel.c_str());
        }
        const std::string second = fits ? firstLine(entry->description)
                                        : attachProblemText(context.registry, entry->fit);
        draw.AddText(ImVec2(position.x, position.y + lineHeight), fits ? dimColor : warningColor, second.c_str());
        draw.PopClipRect();
        ImGui::PopID();
    }

    return chosen;
}

void TypePickerDialog::drawDetails(EditorContext& context, const TypeEntry* entry)
{
    if (!ImGui::BeginChild("details", ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing() * kDetailsLines))) {
        ImGui::EndChild();

        return;
    }
    if (entry) {
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextUnformatted(entry->description.c_str());
        const TypeRegistry& registry = context.registry;
        if (entry->fit.problem != AttachProblem::None) {
            ImGui::TextColored(
                toImVec4(context.theme.colors().warningText),
                "%s",
                attachProblemText(registry, entry->fit).c_str()
            );
        }
        if (!entry->requiredNode.empty()) {
            const std::string nodeName = typeDisplayName(registry, entry->requiredNode);
            ImGui::TextDisabled("%s", trFormat("editor.typePicker.goesOn", nodeName).c_str());
        }
        if (!entry->addsWith.empty()) {
            std::string names;
            for (const std::string& name : entry->addsWith) {
                names += (names.empty() ? "" : ", ") + name;
            }
            ImGui::TextDisabled("%s", trFormat("editor.typePicker.addsWith", names).c_str());
        }
        ImGui::PopTextWrapPos();
        if (!entry->guidePage.empty() && ImGui::SmallButton(tr("editor.typePicker.guide").c_str())) {
            platform::openUrl(guideUrl(entry->guidePage, context.host.language().localization().language()));
        }
    }
    ImGui::EndChild();
}

void TypePickerDialog::add(EditorContext& context, const TypeEntry& entry)
{
    EditorSession& session = context.session;
    if (m_mode == Mode::Nodes) {
        session.editing().createNode(*entry.type, session.findNode(m_target));
    } else if (Node* node = session.findNode(m_target)) {
        session.editing().addComponent(*node, *entry.type);
    }
    if (session.project()) {
        rememberRecentType(context.settings.recentTypesOf(session.project()->directory()), entry.type->name());
        context.settings.save();
    }
}

} // namespace yqlora::editor
