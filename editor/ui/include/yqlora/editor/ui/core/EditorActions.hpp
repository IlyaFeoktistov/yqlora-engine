#pragma once

#include <functional>

namespace yqlora::editor {

class ActionRegistry;
class EditorSession;

// Что делают действия, которые знает только приложение: сохранение с записью
// в лог, закрытие проекта и выход с вопросом о несохранённых изменениях,
// окна настроек проекта и редактора, Play со сборкой скриптов перед ним.
struct EditorActionHandlers {
    std::function<void()> save;
    std::function<void()> closeProject;
    std::function<void()> quit;
    std::function<void()> openProjectSettings;
    std::function<void()> openEditorSettings;
    std::function<void()> toggleMaximizedPanel;
    std::function<void()> buildScripts;
    // Play или Stop, когда игра идёт.
    std::function<void()> togglePlay;
};

// Все действия редактора (см. ActionIds.hpp): подписи, клавиши и доступность.
void registerEditorActions(ActionRegistry& registry, EditorSession& session, EditorActionHandlers handlers);

} // namespace yqlora::editor
