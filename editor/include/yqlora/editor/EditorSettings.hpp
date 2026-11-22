#pragma once

#include "yqlora/debug/DebugChannels.hpp"
#include "yqlora/editor/CodeEditor.hpp"
#include "yqlora/editor/ScriptToolchain.hpp"
#include "yqlora/editor/Snapping.hpp"
#include "yqlora/scene/Node.hpp"

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace yqlora::editor {

// Привычки человека в редакторе: переживают перезапуск, в проект не идут
// (настройки проекта - другое, см. docs/architecture/project-settings.md).
// Файл в папке пользователя, рядом с настройками движка.
class EditorSettings {
public:
    explicit EditorSettings(std::filesystem::path file);

    // Нет файла или он повреждён - остаются значения по умолчанию.
    void load();
    bool save() const;

    // Маленькое окно с картинкой через выбранную камеру в углу окна сцены.
    bool cameraPreview = true;
    // Небо сцены за объектами в окне сцены; выключено - цвет темы. Свет
    // неба сцену освещает в любом случае.
    bool viewportSky = true;
    // Открытый префаб без своего неба и солнца освещается светом новой
    // сцены (PreviewLighting); выключено - только своим.
    bool viewportPreviewLighting = true;
    // Отладка: освещённое в окне сцены подкрашено цветом каскада тени солнца.
    bool viewportShadowCascades = false;
    // Каналы отладочной отрисовки (DebugDraw), которые показывают окно
    // сцены и окно игры: меню "Гизмо" каждого окна.
    DebugChannels sceneDebugChannels{std::string(kDefaultDebugChannel)};
    DebugChannels gameDebugChannels;
    // Что сохранять, когда узел перетаскивают к другому родителю в дереве;
    // nullopt - спрашивать каждый раз. Меняется в меню "Правка".
    std::optional<KeepWorld> reparentKeepWorld;
    // Привязка при перемещении: кнопки на панели окна вида.
    SnapSettings snap;
    // Пути вместо записанных движком в sdk/toolchain.json.
    ToolchainOverrides toolchain;
    // Где открываются файлы кода и места ошибок.
    CodeEditorSetting codeEditor;

    // Последние добавленные типы узлов и компонентов по папкам проектов
    // (полка "Недавние" окна выбора, см. rememberRecentType).
    // TODO: ключ - путь проекта: перенесённый проект теряет список, а списки
    // удалённых проектов копятся. Ключом мог бы стать постоянный ID проекта.
    std::map<std::string, std::vector<std::string>> recentTypes;

    // Недавние типы проекта в папке project; создаёт пустой список.
    std::vector<std::string>& recentTypesOf(const std::filesystem::path& project);

private:
    std::filesystem::path m_file;
};

// Каналы, которые показывает хоть одно окно: их собирает идущая сцена.
DebugChannels viewerDebugChannels(const EditorSettings& settings);

} // namespace yqlora::editor
