#pragma once

#include "yqlora/editor/ScriptToolchain.hpp"
#include "yqlora/editor/Snapping.hpp"
#include "yqlora/scene/Node.hpp"

#include <filesystem>
#include <optional>

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
    // Отладка: освещённое в окне сцены подкрашено цветом каскада тени солнца.
    bool viewportShadowCascades = false;
    // Что сохранять, когда узел перетаскивают к другому родителю в дереве;
    // nullopt - спрашивать каждый раз. Меняется в меню "Правка".
    std::optional<KeepWorld> reparentKeepWorld;
    // Привязка при перемещении: кнопки на панели окна вида.
    SnapSettings snap;
    // Пути вместо записанных движком в sdk/toolchain.json; в интерфейсе их нет,
    // только в файле.
    ToolchainOverrides toolchain;

private:
    std::filesystem::path m_file;
};

} // namespace yqlora::editor
