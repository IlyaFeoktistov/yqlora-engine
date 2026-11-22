#include "yqlora/editor/ui/panels/ProjectSettingsPanel.hpp"

#include "yqlora/editor/ui/widgets/PropertyContext.hpp"
#include "yqlora/editor/ui/widgets/PropertyTable.hpp"

#include "yqlora/core/Utf8Path.hpp"
#include "yqlora/editor/EditorSession.hpp"
#include "yqlora/editor/LayerUsage.hpp"
#include "yqlora/i18n/Translate.hpp"
#include "yqlora/project/SettingsSection.hpp"
#include "yqlora/reflection/TypeRegistry.hpp"

#include <imgui.h>

#include <string>
#include <vector>

namespace yqlora::editor {

namespace {

// Окно открывают поверх остальных, поэтому размер задаётся при первом показе.
constexpr ImVec2 kFirstSize{720.0f, 440.0f};
constexpr float kSectionListWidth = 180.0f;
constexpr const char* kClearLayerPopup = "ClearLayer";

// Ключ перевода имени раздела: editor.settings.<имя типа>.
std::string sectionTitle(const SettingsSection& section)
{
    return tr("editor.settings." + std::string(section.typeName()));
}

} // namespace

void ProjectSettingsPanel::draw(EditorContext& context)
{
    EditorSession& session = context.session;
    if (m_focusRequested) {
        ImGui::SetNextWindowFocus();
        m_focusRequested = false;
    }
    ImGui::SetNextWindowSize(kFirstSize, ImGuiCond_FirstUseEver);
    if (!beginWindow(context)) {
        ImGui::End();

        return;
    }
    if (!session.project() || session.project()->settings().sections().empty()) {
        ImGui::TextDisabled("%s", tr("editor.settings.nothing").c_str());
        ImGui::End();

        return;
    }

    // Версия формата не настройка: её меняет только перевод проекта, поэтому она лишь текстом.
    ImGui::TextDisabled("%s %d", tr("editor.settings.projectVersion").c_str(), Project::formatVersion());
    ImGui::SetItemTooltip("%s", tr("editor.settings.projectVersionHint").c_str());

    ProjectSettings& settings = session.project()->settings();
    SettingsSection* selected = settings.find(m_selected);
    if (!selected) {
        selected = settings.sections().front().get();
    }

    ImGui::BeginChild("sections", ImVec2(kSectionListWidth, 0.0f), ImGuiChildFlags_Borders);
    for (const auto& section : settings.sections()) {
        ImGui::PushID(section.get());
        if (ImGui::Selectable(sectionTitle(*section).c_str(), section.get() == selected)) {
            selected = section.get();
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
    m_selected = std::string(selected->typeName());

    ImGui::SameLine();
    ImGui::BeginChild("properties");
    bool dragging = false;
    if (const TypeInfo* type = session.registry().find(selected->typeName())) {
        dragging = drawPropertyTable(
            session.registry(),
            propertyEditorContext(context),
            *type,
            *selected,
            [this, &session, selected](const Property& property, const PropertyValue& value) {
                if (!askBeforeClearingLayers(session, *selected, property, value)) {
                    session.settingsEditing().setProperty(*selected, property, value);
                }
            }
        );
    }
    ImGui::EndChild();
    drawPendingQuestion(session);
    ImGui::End();
    context.mouseWrapping = context.mouseWrapping || dragging;
}

bool ProjectSettingsPanel::askBeforeClearingLayers(
    EditorSession& session,
    const SettingsSection& section,
    const Property& property,
    const PropertyValue& value
)
{
    using Names = PropertyStorage<std::vector<std::string>>;
    if (property.hint() != PropertyHint::LayerNames) {
        return false;
    }
    const auto before = Names::load(std::get<ListValue>(property.get(section)));
    const auto after = Names::load(std::get<ListValue>(value));

    std::string question;
    for (const int layer : clearedLayers(before, after)) {
        const LayerUsage usage = layerUsage(session, layer);
        if (usage.nodes == 0) {
            continue;
        }
        question += trFormat("editor.settings.layerNodes", before[static_cast<std::size_t>(layer)], usage.nodes) + " ";
        for (std::size_t i = 0; i < usage.scenes.size(); ++i) {
            question += (i == 0 ? "" : ", ") + toUtf8(usage.scenes[i]);
        }
        question += "\n";
    }
    if (question.empty()) {
        return false;
    }
    m_pending = PendingEdit{
        .section = std::string(section.typeName()),
        .property = &property,
        .value = value,
        .question = std::move(question),
    };

    return true;
}

void ProjectSettingsPanel::drawPendingQuestion(EditorSession& session)
{
    if (m_pending && !ImGui::IsPopupOpen(kClearLayerPopup)) {
        ImGui::OpenPopup(kClearLayerPopup);
    }
    const std::string title = tr("editor.settings.clearLayerTitle") + "###" + kClearLayerPopup;
    if (!ImGui::BeginPopupModal(title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }
    ImGui::TextUnformatted(tr("editor.settings.clearLayerQuestion").c_str());
    ImGui::Spacing();
    ImGui::TextUnformatted(m_pending ? m_pending->question.c_str() : "");
    ImGui::TextDisabled("%s", tr("editor.settings.clearLayerKeeps").c_str());
    ImGui::Spacing();

    bool close = !m_pending;
    if (ImGui::Button(tr("editor.settings.clearLayer").c_str()) && m_pending) {
        SettingsSection* section = session.project() ? session.project()->settings().find(m_pending->section) : nullptr;
        if (section) {
            session.settingsEditing().setProperty(*section, *m_pending->property, m_pending->value);
            session.finishEdit();
        }
        close = true;
    }
    ImGui::SameLine();
    if (ImGui::Button(tr("editor.common.cancel").c_str())) {
        close = true;
    }
    if (close) {
        m_pending.reset();
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

} // namespace yqlora::editor
