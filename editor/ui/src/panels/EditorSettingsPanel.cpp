#include "yqlora/editor/ui/panels/EditorSettingsPanel.hpp"

#include "yqlora/editor/ui/core/EditorHost.hpp"
#include "yqlora/editor/ui/style/Icons.hpp"
#include "yqlora/editor/ui/style/Theme.hpp"

#include "yqlora/core/Utf8Path.hpp"
#include "yqlora/editor/CodeEditor.hpp"
#include "yqlora/editor/EditorSettings.hpp"
#include "yqlora/i18n/Translate.hpp"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <filesystem>
#include <string>

namespace yqlora::editor {

namespace {

// Окно открывают поверх остальных, поэтому размер задаётся при первом показе.
constexpr ImVec2 kFirstSize{640.0f, 400.0f};
constexpr float kLabelColumnWidth = 170.0f;
constexpr int kColumns = 2;

// Шаблон проверяется на месте с таким файлом: ошибка видна сразу, а не при первом открытии.
constexpr const char* kSampleFile = "Example.cpp";

void beginRow(const char* labelKey)
{
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(tr(labelKey).c_str());
    ImGui::TableNextColumn();
}

bool beginFields(const char* id)
{
    if (!ImGui::BeginTable(id, kColumns, ImGuiTableFlags_SizingStretchProp)) {
        return false;
    }
    ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, kLabelColumnWidth);
    ImGui::TableSetupColumn("field", ImGuiTableColumnFlags_WidthStretch);

    return true;
}

// Поле пути с кнопкой выбора файла. hint виден, пока поле пусто. Правка
// пишется в файл, когда поле отпускают, и сразу после выбора в диалоге.
void pathField(EditorContext& context, const char* id, std::filesystem::path& path, const std::string& hint)
{
    ImGui::PushID(id);
    const float buttonWidth = ImGui::GetFrameHeight();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - buttonWidth - ImGui::GetStyle().ItemSpacing.x);
    std::string text = toUtf8(path);
    if (ImGui::InputTextWithHint("##path", hint.c_str(), &text)) {
        path = fromUtf8(text);
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        context.settings.save();
    }
    ImGui::SameLine();
    if (ImGui::Button(icons::kFolderOpen, ImVec2(buttonWidth, 0.0f))) {
        EditorSettings& settings = context.settings;
        // Имя для поиска в PATH - не место на диске: тогда диалог начинает там, где решит система.
        const std::filesystem::path start = path.is_absolute() ? path : std::filesystem::path();
        context.host.showFileDialog(start, [&settings, &path](std::optional<std::filesystem::path> chosen) {
            if (chosen) {
                path = std::move(*chosen);
                settings.save();
            }
        });
    }
    ImGui::SetItemTooltip("%s", tr("editor.editorSettings.browse").c_str());
    ImGui::PopID();
}

} // namespace

void EditorSettingsPanel::draw(EditorContext& context)
{
    if (m_focusRequested) {
        ImGui::SetNextWindowFocus();
        m_focusRequested = false;
    }
    ImGui::SetNextWindowSize(kFirstSize, ImGuiCond_FirstUseEver);
    if (!beginWindow(context)) {
        ImGui::End();

        return;
    }
    drawCodeEditor(context);
    drawToolchain(context);
    ImGui::End();
}

void EditorSettingsPanel::drawCodeEditor(EditorContext& context)
{
    ImGui::SeparatorText(tr("editor.editorSettings.codeEditor").c_str());
    CodeEditorSetting& setting = context.settings.codeEditor;
    if (!beginFields("codeEditor")) {
        return;
    }

    beginRow("editor.editorSettings.preset");
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("##preset", tr("editor.editorSettings.presetChoose").c_str())) {
        for (const CodeEditorPreset& preset : codeEditorPresets()) {
            if (ImGui::Selectable(std::string(preset.name).c_str())) {
                setting.program = fromUtf8(preset.program);
                setting.arguments = std::string(preset.arguments);
                context.settings.save();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SetItemTooltip("%s", tr("editor.editorSettings.presetHint").c_str());

    beginRow("editor.editorSettings.program");
    pathField(context, "program", setting.program, tr("editor.editorSettings.programSystem"));

    beginRow("editor.editorSettings.arguments");
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputText("##arguments", &setting.arguments);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        context.settings.save();
    }
    ImGui::SetItemTooltip("%s", tr("editor.editorSettings.argumentsHint").c_str());
    ImGui::EndTable();

    // Пока программа не задана, шаблон не используется: ошибку не показываем.
    if (!setting.program.empty()) {
        const CodeEditorArguments check = expandCodeEditorArguments(
            setting.arguments,
            {.file = kSampleFile, .line = 1, .column = 1},
            {}
        );
        if (!check.ok()) {
            ImGui::PushStyleColor(ImGuiCol_Text, toImVec4(context.theme.colors().errorText));
            ImGui::TextWrapped("%s %s", tr(check.error).c_str(), check.detail.c_str());
            ImGui::PopStyleColor();
        }
    }
}

void EditorSettingsPanel::drawToolchain(EditorContext& context)
{
    ImGui::SeparatorText(tr("editor.editorSettings.toolchain").c_str());
    ImGui::TextDisabled("%s", tr("editor.editorSettings.toolchainHint").c_str());
    if (!beginFields("toolchain")) {
        return;
    }
    for (const ToolchainOverrideField& field : kToolchainOverrideFields) {
        const std::string labelKey = std::string("editor.editorSettings.") + field.key;
        beginRow(labelKey.c_str());
        // Пустое поле показывает путь из SDK, который и будет взят.
        const std::string recorded = m_sdkToolchain ? toUtf8((*m_sdkToolchain).*field.recorded) : std::string();
        pathField(context, field.key, context.settings.toolchain.*field.override, recorded);
    }
    ImGui::EndTable();
}

} // namespace yqlora::editor
