#pragma once

#include "yqlora/editor/ui/core/EditorPanel.hpp"

#include "yqlora/editor/LogBuffer.hpp"

#include <cstddef>
#include <string_view>
#include <vector>

namespace yqlora::editor {

struct EditorColors;

// Лог движка и сборки скриптов внизу редактора: фильтр по уровню, поиск,
// очистка, копирование. Пока список прокручен до конца, он следует за
// новыми записями. Строка с местом "файл:строка" открывает его щелчком в
// редакторе кода.
class LogPanel final : public EditorPanel {
public:
    explicit LogPanel(LogBuffer& buffer)
        : m_buffer(buffer)
    {
    }

    const char* id() const override
    {
        return "Log";
    }

    const char* titleKey() const override
    {
        return "editor.window.log";
    }

    void draw(EditorContext& context) override;

    // Показать панель и вывести её вперёд, например по щелчку на счётчике ошибок.
    void reveal()
    {
        setVisible(true);
        m_focusRequested = true;
    }

private:
    void drawToolbar(const EditorColors& colors);
    void drawEntries(EditorContext& context);
    // Одна строка текста записи; с местом - щелчок открывает его.
    static void drawLine(EditorContext& context, const LogEntry& entry, std::string_view line);

    LogBuffer& m_buffer;
    LogFilter m_filter;
    // Записи, прошедшие фильтр, номерами в m_buffer.entries().
    std::vector<std::size_t> m_shown;
    // LogBuffer::received на прошлом кадре: пришли ли новые записи.
    std::size_t m_seenReceived = 0;
    bool m_focusRequested = false;
};

} // namespace yqlora::editor
