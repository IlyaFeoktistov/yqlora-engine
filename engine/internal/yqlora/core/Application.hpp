#pragma once

#include "yqlora/core/EngineServices.hpp"
#include "yqlora/core/Export.hpp"
#include "yqlora/input/Input.hpp"
#include "yqlora/math/Math.hpp"
#include "yqlora/platform/Window.hpp"
#include "yqlora/reflection/TypeRegistry.hpp"
#include "yqlora/render/GraphicsApi.hpp"
#include "yqlora/resource/ResourceId.hpp"

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace yqlora {

class FileDialogs;
class Gamepads;
class GameRuntime;
class Language;
class Renderer;
class UiLayer;

enum class ApplicationMode {
    // Игра: каждый кадр GameRuntime двигает свою сцену и рисует её в окно.
    Game,
    // Инструмент (редактор): окно целиком занимает интерфейс на Dear ImGui,
    // сцены инструмент рисует сам, в onGui.
    Tool,
};

struct ApplicationConfig {
    // Латиница, без пробелов: из имени строится путь к папке настроек.
    std::string name = "YqloraApp";
    // Ключ перевода для заголовка окна.
    std::string titleKey = "app.title";
    int width = kDefaultWindowWidth;
    int height = kDefaultWindowHeight;
    GraphicsApi api = GraphicsApi::OpenGL;
    ApplicationMode mode = ApplicationMode::Game;
};

// Собирает части приложения и крутит цикл кадра: окно, рендерер, ввод,
// язык, реестр типов и службы движка. Сцену держит GameRuntime (режим игры)
// или сам инструмент. Игра или инструмент наследуются и переопределяют on*-методы.
class YQLORA_API Application {
public:
    explicit Application(ApplicationConfig config);
    virtual ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    // Запускает цикл до закрытия окна. Возвращает код завершения процесса.
    int run();
    void quit()
    {
        m_running = false;
    }

    // Типы узлов и компонентов: движка (регистрируются сразу) и игры.
    TypeRegistry& registry()
    {
        return m_registry;
    }

    const TypeRegistry& registry() const
    {
        return m_registry;
    }

    // Службы для сцен приложения: ввод, реестр, открытый проект.
    EngineServices& services()
    {
        return m_services;
    }

    const Input& input() const
    {
        return m_input;
    }

    // Только в режиме игры и только во время run().
    GameRuntime* game()
    {
        return m_game.get();
    }

    // Только во время run().
    Language& language()
    {
        return *m_language;
    }

    Renderer& renderer()
    {
        return *m_renderer;
    }

    // См. Window::setMouseWrapping.
    void setMouseWrapping(bool wrapping);

    // Размер окна в тех же единицах, что положение мыши.
    Vec2 windowSize() const;

    // Ширина к высоте области вывода: нужна, чтобы построить проекцию своей камеры.
    float aspectRatio() const;

    // Уточнение в заголовке окна перед названием программы, например имя проекта.
    // Пустая строка - только название.
    void setTitleDetail(std::string detail);

    // См. FileDialogs::showFolderDialog.
    void showFolderDialog(
        const std::filesystem::path& start,
        std::function<void(std::optional<std::filesystem::path>)> callback
    );

protected:
    virtual void onStart()
    {
    }

    virtual void onUpdate(float /*deltaSeconds*/)
    {
    }

    // Окно просят закрыть (крестик, Alt+F4). По умолчанию сразу выход;
    // приложение может сначала спросить про несохранённые изменения.
    virtual void onCloseRequested()
    {
        quit();
    }

    // Окно стало активным: человек вернулся из другой программы (например, из
    // IDE, где правил скрипты).
    virtual void onFocusGained()
    {
    }

    // Только в режиме инструмента: строит интерфейс кадра вызовами ImGui.
    // Шрифт и тему стоит задать в onStart, до первого кадра.
    virtual void onGui()
    {
    }

    virtual void onShutdown()
    {
    }

    // Рендерер сейчас пересоздадут (сменили графический API в настройках):
    // всё, что создано через него (цели отрисовки, текстуры), надо отпустить.
    // Новые создаются уже у нового рендерера.
    virtual void onRendererLost()
    {
    }

private:
    // Стадии кадра для FrameLoop: общие и свои у каждого режима.
    class Stages;
    class GameStages;
    class ToolStages;

    // Заголовок окна на текущем языке; меняется, только если стал другим.
    void updateTitle();
    // Раздел Graphics открытого проекта: vsync сразу, смена API - пересоздание рендера.
    void applyGraphicsSettings();
    // Только в режиме игры: иконка окна из раздела General. У редактора своя.
    void applyGameIcon();
    // Пересоздаёт только рендер: окно (если API нужны другие флаги окна),
    // рендерер и отрисовку интерфейса. Сцены, проект и интерфейс остаются.
    void rebuildRenderer(GraphicsApi api);
    void mainLoop();

    ApplicationConfig m_config;
    TypeRegistry m_registry;
    Input m_input;
    EngineServices m_services{m_input, m_registry};
    std::unique_ptr<Language> m_language;
    std::unique_ptr<Window> m_window;
    std::unique_ptr<FileDialogs> m_dialogs;
    // Живут дольше окна: оно пересоздаётся при смене графического API.
    std::unique_ptr<Gamepads> m_gamepads;
    std::unique_ptr<Renderer> m_renderer;
    // Только в режиме инструмента.
    std::unique_ptr<UiLayer> m_ui;
    // Только в режиме игры. Создаётся в run(): к этому времени игра
    // зарегистрировала свои типы, и системы сцены берутся из полного реестра.
    std::unique_ptr<GameRuntime> m_game;
    bool m_running = false;
    // Какой vsync уже включён; nullopt - ещё не включали у этого рендерера.
    std::optional<bool> m_vsync;
    // API из настроек, которого нет в сборке или в системе: о нём уже сказали в лог.
    std::optional<GraphicsApi> m_unavailableApi;
    // Иконка, которая уже стоит у окна: ID текстуры и её ревизия.
    ResourceId m_iconId = 0;
    std::uint64_t m_iconRevision = 0;
    std::string m_titleDetail;
    std::string m_title;
};

} // namespace yqlora
