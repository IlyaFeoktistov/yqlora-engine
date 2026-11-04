#include "yqlora/core/Application.hpp"
#include "yqlora/project/GeneralSettings.hpp"
#include "yqlora/project/Project.hpp"
#include "yqlora/render/TextureData.hpp"
#include "yqlora/resource/ResourceCache.hpp"

#include "yqlora/core/AppPaths.hpp"
#include "yqlora/core/EngineTypes.hpp"
#include "yqlora/core/FrameLoop.hpp"
#include "yqlora/core/Log.hpp"
#include "yqlora/core/LogSettings.hpp"
#include "yqlora/core/Utf8Path.hpp"
#include "yqlora/i18n/Language.hpp"
#include "yqlora/input/InputSettings.hpp"
#include "yqlora/platform/FileDialogs.hpp"
#include "yqlora/platform/Gamepads.hpp"
#include "yqlora/platform/InputFeeder.hpp"
#include "yqlora/platform/Platform.hpp"
#include "yqlora/platform/Window.hpp"
#include "yqlora/render/GraphicsBackend.hpp"
#include "yqlora/render/GraphicsSettings.hpp"
#include "yqlora/render/Renderer.hpp"
#include "yqlora/runtime/GameRuntime.hpp"
#include "yqlora/scene/TimeSettings.hpp"
#include "yqlora/ui/UiLayer.hpp"

#include <algorithm>
#include <exception>

#ifdef _WIN32
// На ноутбуках с двумя видеокартами драйвер по умолчанию запускает программу
// на встроенной. По этим экспортам из exe драйверы NVIDIA и AMD выбирают
// дискретную. Код приложения всегда подтягивает этот файл, поэтому экспорты
// попадают в любой exe на движке. Пользователь может переопределить выбор
// в настройках графики Windows.
extern "C" {
__declspec(dllexport) unsigned long NvOptimusEnablement = 1;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

namespace yqlora {

namespace {

constexpr const char* kSettingsFile = "settings.json";
constexpr const char* kUiLayoutFile = "imgui.ini";
constexpr const char* kLocalesFolder = "locales";
// Пока окно ещё не нарисовано интерфейсом.
constexpr Vec4 kWindowClearColor{0.1f, 0.1f, 0.11f, 1.0f};
constexpr std::string_view kTitleSeparator = " - ";

} // namespace

Application::Application(ApplicationConfig config)
    : m_config(std::move(config))
{
    registerEngineTypes(m_registry);
}

Application::~Application() = default;

int Application::run()
{
    const auto userData = userDataDirectory(m_config.name);
    log::openFile(userData / "yqlora.log");
    if (!platform::initialize()) {
        return 1;
    }

    int exitCode = 0;
    try {
        m_language = std::make_unique<Language>(assetsDirectory() / kLocalesFolder, userData / kSettingsFile);
        prepareWindowForApi(m_config.api);
        m_window = std::make_unique<Window>(WindowDesc{
            .title = tr(m_config.titleKey),
            .width = m_config.width,
            .height = m_config.height,
            .api = m_config.api,
        });
        m_dialogs = std::make_unique<FileDialogs>(*m_window);
        m_gamepads = std::make_unique<Gamepads>();
        m_renderer = createRenderer(m_config.api, *m_window);
        if (m_config.mode == ApplicationMode::Tool) {
            m_ui = std::make_unique<UiLayer>(*m_renderer, userData / kUiLayoutFile);
        } else {
            m_game = std::make_unique<GameRuntime>(m_services);
        }
        updateTitle();

        onStart();
        mainLoop();
        onShutdown();
    } catch (const std::exception& e) {
        log::error("Fatal: {}", e.what());
        exitCode = 1;
    }

    m_game.reset();
    m_ui.reset();
    // Рендерер держит контекст окна, поэтому уничтожаем строго в обратном порядке.
    m_renderer.reset();
    m_gamepads.reset();
    m_dialogs.reset();
    m_window.reset();
    m_language.reset();
    platform::shutdown();

    return exitCode;
}

void Application::setTitleDetail(std::string detail)
{
    m_titleDetail = std::move(detail);
    updateTitle();
}

void Application::updateTitle()
{
    if (!m_window) {
        return;
    }
    std::string title = tr(m_config.titleKey);
    if (!m_titleDetail.empty()) {
        title = m_titleDetail + std::string(kTitleSeparator) + title;
    }
    if (title != m_title) {
        m_title = std::move(title);
        m_window->setTitle(m_title);
    }
}

void Application::showFolderDialog(
    const std::filesystem::path& start,
    std::function<void(std::optional<std::filesystem::path>)> callback
)
{
    m_dialogs->showFolderDialog(start, std::move(callback));
}

void Application::setMouseWrapping(bool wrapping)
{
    m_window->setMouseWrapping(wrapping);
}

Vec2 Application::windowSize() const
{
    return m_window->sizeInPoints();
}

float Application::aspectRatio() const
{
    const int width = m_window->widthInPixels();
    const int height = std::max(m_window->heightInPixels(), 1);

    return static_cast<float>(width) / static_cast<float>(height);
}

// Общее у обоих режимов: ввод и вывод кадра на экран.
class Application::Stages : public FrameStages {
public:
    explicit Stages(Application& app)
        : m_app(app)
        , m_input(app.m_input)
    {
        m_events.onCloseRequested = [&app] {
            app.onCloseRequested();
        };
        m_events.onResized = [&app](int width, int height) {
            app.m_renderer->resize(width, height);
        };
        m_events.onFocusGained = [&app] {
            app.onFocusGained();
        };
    }

    bool input() override
    {
        m_input.beginFrame();
        m_app.m_dialogs->deliver();
        m_app.m_window->pollEvents(m_events, m_input, *m_app.m_gamepads);
        m_input.updateActions(inputSettingsOf(m_app.m_services));

        return m_app.m_running;
    }

    void present() override
    {
        // Язык могли переключить за кадр.
        m_app.updateTitle();
        m_app.m_renderer->present();
    }

protected:
    Application& m_app;
    InputFeeder m_input;
    WindowEvents m_events;
};

class Application::GameStages final : public Stages {
public:
    using Stages::Stages;

    void simulate(float deltaSeconds) override
    {
        m_app.m_game->simulate(deltaSeconds);
        m_app.onUpdate(deltaSeconds);
    }

    void updateTransforms() override
    {
        m_app.m_game->updateTransforms();
    }

    void extract() override
    {
        m_frame = m_app.m_game->extract(m_app.aspectRatio());
    }

    void render() override
    {
        m_app.m_renderer->render(m_frame);
    }

    void interface() override
    {
    }

private:
    FrameData m_frame;
};

// Сцены инструмент рисует сам, в своих окнах во время интерфейса: у каждого
// окна своя камера и своя текстура.
class Application::ToolStages final : public Stages {
public:
    explicit ToolStages(Application& app)
        : Stages(app)
    {
        m_events.onEvent = [&app](const SDL_Event& event) {
            app.m_ui->processEvent(event);
        };
    }

    bool input() override
    {
        const bool running = Stages::input();
        if (m_app.m_window->mouseWrapping()) {
            m_app.m_ui->setMouseDelta(m_app.m_input.mousePosition(), m_app.m_input.mouseDelta());
        }

        return running;
    }

    void simulate(float deltaSeconds) override
    {
        m_app.onUpdate(deltaSeconds);
    }

    void updateTransforms() override
    {
    }

    void extract() override
    {
    }

    void render() override
    {
    }

    void interface() override
    {
        m_app.m_ui->beginFrame();
        m_app.onGui();
        m_app.m_renderer->clearWindow(kWindowClearColor);
        m_app.m_ui->endFrame();
    }
};

void Application::applyGraphicsSettings()
{
    const GraphicsSettings& graphics = graphicsSettingsOf(m_services);
    const GraphicsApi api = graphics.currentPlatformApi();
    if (api != m_renderer->api() && api != m_unavailableApi) {
        if (isBuilt(api)) {
            rebuildRenderer(api);
        } else {
            log::warn(
                "Graphics API {} is not built into this engine, staying on {}",
                enumName(api),
                enumName(m_renderer->api())
            );
            m_unavailableApi = api;
        }
    }
    if (graphics.vsync != m_vsync) {
        m_renderer->setVsync(graphics.vsync);
        m_vsync = graphics.vsync;
    }
}

void Application::applyGameIcon()
{
    const Project* project = m_services.project();
    const ResourceId id = project ? project->settings().get<GeneralSettings>().icon.id : 0;
    const LoadedData<TextureData> texture = m_services.resources().get<TextureData>(id);
    if (id == m_iconId && texture.revision == m_iconRevision) {
        return;
    }
    m_iconId = id;
    m_iconRevision = texture.revision;
    // Без иконки в настройках остаётся та, что уже стоит: SDL не умеет вернуть
    // иконку системы.
    if (texture) {
        m_window->setIcon(texture.data->width, texture.data->height, displayPixels(*texture.data));
    }
}

void Application::rebuildRenderer(GraphicsApi api)
{
    const GraphicsApi previous = m_renderer->api();
    log::info("Graphics API: {} -> {}", enumName(previous), enumName(api));

    // Сначала всё, что создано через рендерер, потом он сам: ресурсы
    // удаляются, пока жив их контекст.
    onRendererLost();
    if (m_ui) {
        m_ui->releaseRenderer();
    }
    m_renderer.reset();

    const auto create = [this](GraphicsApi target) {
        prepareWindowForApi(target);
        if (!windowFitsApi(*m_window, target)) {
            const Vec2 size = m_window->sizeInPoints();
            m_dialogs.reset();
            m_window = std::make_unique<Window>(WindowDesc{
                .title = m_title,
                .width = static_cast<int>(size.x),
                .height = static_cast<int>(size.y),
                .api = target,
            });
            m_dialogs = std::make_unique<FileDialogs>(*m_window);
            // У нового окна иконки ещё нет.
            m_iconId = 0;
            m_iconRevision = 0;
        }
        m_renderer = createRenderer(target, *m_window);
    };
    try {
        create(api);
    } catch (const std::exception& e) {
        // API собран, но система его не дала: остаёмся на прежнем.
        log::error("Graphics API {} cannot start: {}. Staying on {}", enumName(api), e.what(), enumName(previous));
        m_unavailableApi = api;
        create(previous);
    }
    if (m_ui) {
        m_ui->attachRenderer(*m_renderer);
    }
    m_vsync.reset();
}

void Application::mainLoop()
{
    std::unique_ptr<FrameStages> stages;
    if (m_config.mode == ApplicationMode::Tool) {
        stages = std::make_unique<ToolStages>(*this);
    } else {
        stages = std::make_unique<GameStages>(*this);
    }
    FrameLoop loop(*stages, timeSettingsOf(m_services).maxFrame());

    m_running = true;
    loop.reset(platform::timeSeconds());
    while (m_running) {
        applyGraphicsSettings();
        if (m_config.mode == ApplicationMode::Game) {
            applyGameIcon();
        }
        loop.setMaxFrameSeconds(timeSettingsOf(m_services).maxFrame());
        loop.runFrame(platform::timeSeconds());
    }
}

} // namespace yqlora
