#pragma once

#include "yqlora/editor/ui/core/EditorPanel.hpp"

#include "yqlora/editor/ScriptToolchain.hpp"

#include <optional>

namespace yqlora::editor {

// Настройки редактора (Правка -> Настройки редактора): привычки человека,
// общие для всех проектов. Правка сразу пишется в файл настроек редактора,
// без истории правок: к сцене она не относится. Открывается из меню, в
// начале скрыто.
class EditorSettingsPanel final : public EditorPanel {
public:
    // sdkToolchain - запись SDK без переопределений: её пути видны в пустых полях.
    explicit EditorSettingsPanel(const std::optional<ScriptToolchain>& sdkToolchain)
        : m_sdkToolchain(sdkToolchain)
    {
        setVisible(false);
    }

    const char* id() const override
    {
        return "EditorSettings";
    }

    const char* titleKey() const override
    {
        return "editor.window.editorSettings";
    }

    void open()
    {
        setVisible(true);
        m_focusRequested = true;
    }

    void draw(EditorContext& context) override;

private:
    void drawCodeEditor(EditorContext& context);
    void drawToolchain(EditorContext& context);

    const std::optional<ScriptToolchain>& m_sdkToolchain;
    bool m_focusRequested = false;
};

} // namespace yqlora::editor
