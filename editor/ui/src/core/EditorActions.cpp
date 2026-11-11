#include "yqlora/editor/ui/core/EditorActions.hpp"

#include "yqlora/editor/ui/core/ActionIds.hpp"
#include "yqlora/editor/ui/core/Actions.hpp"

#include "yqlora/editor/EditorSession.hpp"

#include <imgui.h>

#include <utility>

namespace yqlora::editor {

void registerEditorActions(ActionRegistry& registry, EditorSession& session, EditorActionHandlers handlers)
{
    const auto hasProject = [&session] {
        return session.project().has_value();
    };
    const auto playing = [&session] {
        return session.playing();
    };

    registry.add(actions::kSave, "editor.menu.save")
        .shortcut(ImGuiMod_Ctrl | ImGuiKey_S)
        // Во время игры в сцене её состояние, а не то, что правили: сохранять нечего.
        .enabledWhen([&session] {
            return session.project() && !session.playing();
        })
        .onRun(std::move(handlers.save));
    registry.add(actions::kCloseProject, "editor.menu.closeProject")
        .enabledWhen(hasProject)
        .onRun(std::move(handlers.closeProject));
    registry.add(actions::kProjectSettings, "editor.menu.projectSettings")
        .enabledWhen(hasProject)
        .onRun(std::move(handlers.openProjectSettings));
    registry.add(actions::kEditorSettings, "editor.menu.editorSettings").onRun(std::move(handlers.openEditorSettings));
    registry.add(actions::kQuit, "editor.menu.quit").onRun(std::move(handlers.quit));

    registry.add(actions::kUndo, "editor.menu.undo")
        .shortcut(ImGuiMod_Ctrl | ImGuiKey_Z)
        .enabledWhen([&session] {
            return session.canUndo();
        })
        .onRun([&session] {
            session.undo();
        });
    registry.add(actions::kRedo, "editor.menu.redo")
        .shortcut(ImGuiMod_Ctrl | ImGuiKey_Y)
        .alternativeShortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z)
        .enabledWhen([&session] {
            return session.canRedo();
        })
        .onRun([&session] {
            session.redo();
        });

    // Развернуть окно в фокусе на всё окно редактора или вернуть раскладку.
    registry.add(actions::kMaximizePanel, "editor.menu.maximizePanel")
        .shortcut(ImGuiMod_Shift | ImGuiKey_Space)
        .enabledWhen(hasProject)
        .onRun(std::move(handlers.toggleMaximizedPanel));

    registry.add(actions::kTogglePlay, "editor.play.play")
        .shortcut(ImGuiMod_Ctrl | ImGuiKey_P)
        .enabledWhen(hasProject)
        .onRun(std::move(handlers.togglePlay));
    registry.add(actions::kBuildScripts, "editor.menu.buildScripts")
        .shortcut(ImGuiMod_Ctrl | ImGuiKey_B)
        .enabledWhen(hasProject)
        .onRun(std::move(handlers.buildScripts));
    registry.add(actions::kPause, "editor.play.pause").enabledWhen(playing).onRun([&session] {
        session.setPaused(!session.playMode().paused());
    });
    registry.add(actions::kStop, "editor.play.stop").enabledWhen(playing).onRun([&session] {
        session.stop();
    });
}

} // namespace yqlora::editor
