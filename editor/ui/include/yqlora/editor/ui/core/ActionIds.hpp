#pragma once

namespace yqlora::editor::actions {

// ID действий редактора: по ним действия регистрируются и вызываются из меню и кнопок.
inline constexpr const char* kSave = "project.save";
inline constexpr const char* kCloseProject = "project.close";
inline constexpr const char* kProjectSettings = "project.settings";
inline constexpr const char* kQuit = "app.quit";
inline constexpr const char* kUndo = "edit.undo";
inline constexpr const char* kRedo = "edit.redo";
inline constexpr const char* kEditorSettings = "edit.editorSettings";
inline constexpr const char* kMaximizePanel = "view.maximizePanel";
inline constexpr const char* kTogglePlay = "play.toggle";
inline constexpr const char* kPause = "play.pause";
inline constexpr const char* kStop = "play.stop";
inline constexpr const char* kBuildScripts = "scripts.build";

} // namespace yqlora::editor::actions
