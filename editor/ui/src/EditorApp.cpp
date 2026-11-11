#include "yqlora/editor/ui/EditorApp.hpp"

#include "yqlora/editor/ui/core/ActionIds.hpp"
#include "yqlora/editor/ui/core/DefaultLayout.hpp"
#include "yqlora/editor/ui/core/EditorActions.hpp"
#include "yqlora/editor/ui/menu/MainMenu.hpp"

#include "yqlora/editor/ui/panels/EditorSettingsPanel.hpp"
#include "yqlora/editor/ui/panels/FileSystemPanel.hpp"
#include "yqlora/editor/ui/panels/GamePanel.hpp"
#include "yqlora/editor/ui/panels/InspectorPanel.hpp"
#include "yqlora/editor/ui/panels/SceneTreePanel.hpp"
#include "yqlora/editor/ui/panels/ViewportPanel.hpp"
#include "yqlora/editor/ui/style/Icons.hpp"
#include "yqlora/editor/ui/style/Theme.hpp"

#include "yqlora/core/AppPaths.hpp"
#include "yqlora/core/Log.hpp"
#include "yqlora/core/Utf8Path.hpp"
#include "yqlora/editor/ProjectSetup.hpp"
#include "yqlora/i18n/Language.hpp"
#include "yqlora/i18n/Translate.hpp"
#include "yqlora/platform/Platform.hpp"
#include "yqlora/scripting/ScriptsSettings.hpp"

#include <imgui.h>
// ImGuizmo.h использует типы ImGui и должен идти после imgui.h.
#include <ImGuizmo.h>

namespace yqlora::editor {

namespace {

// Из имени строится папка настроек редактора, поэтому латиница без пробелов.
constexpr const char* kApplicationName = "Editor";
constexpr const char* kTitleKey = "editor.title";
constexpr int kWindowWidth = 1600;
constexpr int kWindowHeight = 900;
constexpr const char* kThemeFile = "themes/dark.json";
constexpr const char* kProjectListFile = "projects.json";
constexpr const char* kEditorSettingsFile = "editor.json";

} // namespace

EditorApp::EditorApp()
    : Application({
          .name = kApplicationName,
          .titleKey = kTitleKey,
          .width = kWindowWidth,
          .height = kWindowHeight,
          .mode = ApplicationMode::Tool,
      })
{
    m_panels.push_back(std::make_unique<SceneTreePanel>());
    m_panels.push_back(std::make_unique<InspectorPanel>());
    m_panels.push_back(std::make_unique<FileSystemPanel>());
    auto logPanel = std::make_unique<LogPanel>(m_log);
    m_logPanel = logPanel.get();
    m_panels.push_back(std::move(logPanel));
    m_panels.push_back(std::make_unique<ViewportPanel>());
    m_panels.push_back(std::make_unique<GamePanel>());
    auto projectSettings = std::make_unique<ProjectSettingsPanel>();
    m_projectSettings = projectSettings.get();
    m_panels.push_back(std::move(projectSettings));
    auto editorSettings = std::make_unique<EditorSettingsPanel>(m_toolchain);
    m_editorSettings = editorSettings.get();
    m_panels.push_back(std::move(editorSettings));
}

EditorContext EditorApp::context()
{
    return {
        .host = m_host,
        .session = m_session,
        .registry = registry(),
        .serializer = m_serializer,
        .actions = m_actions,
        .theme = m_theme,
        .settings = *m_settings,
        .deltaSeconds = m_deltaSeconds,
        .openScene =
            [this](const std::filesystem::path& path) {
                openScene(path);
            },
        .openProjectSettings =
            [this](std::string_view section) {
                m_projectSettings->open(section);
            },
        .maximized = m_maximized,
        .toggleMaximized =
            [this](EditorPanel& panel) {
                toggleMaximized(panel);
            },
    };
}

void EditorApp::onStart()
{
    const auto assets = assetsDirectory();
    const auto userData = userDataDirectory(kApplicationName);
    m_theme.load(assets / kThemeFile, assets);

    const Localization& localization = language().localization();
    for (const std::string& code : localization.availableLanguages()) {
        m_languages.emplace_back(code, localization.displayName(code));
    }

    m_projects = std::make_unique<ProjectManager>(userData / kProjectListFile, m_serializer);
    m_settings = std::make_unique<EditorSettings>(userData / kEditorSettingsFile);
    m_settings->load();
    m_toolchain = loadScriptToolchain(sdkDirectory());
    m_projectManager = std::make_unique<ProjectManagerPanel>(*m_projects, m_theme);

    registerEditorActions(
        m_actions,
        m_session,
        {
            .save =
                [this] {
                    save();
                },
            .closeProject =
                [this] {
                    m_session.refreshModified();
                    m_unsaved.request(m_session.modified(), [this] {
                        closeProject();
                    });
                },
            .quit =
                [this] {
                    m_session.refreshModified();
                    m_unsaved.request(m_session.modified(), [this] {
                        quit();
                    });
                },
            .openProjectSettings =
                [this] {
                    m_projectSettings->open();
                },
            .openEditorSettings =
                [this] {
                    m_editorSettings->open();
                },
            .toggleMaximizedPanel =
                [this] {
                    toggleMaximizedFocused();
                },
            .buildScripts =
                [this] {
                    buildScripts(false);
                },
            .togglePlay =
                [this] {
                    togglePlay();
                },

        }
    );
}

void EditorApp::toggleMaximized(EditorPanel& panel)
{
    m_maximized = m_maximized == &panel ? nullptr : &panel;
}

void EditorApp::toggleMaximizedFocused()
{
    if (m_maximized) {
        m_maximized = nullptr;

        return;
    }
    for (const auto& panel : m_panels) {
        if (panel->visible() && panel->focused()) {
            m_maximized = panel.get();

            return;
        }
    }
}

void EditorApp::onRendererLost()
{
    for (const auto& panel : m_panels) {
        panel->onRendererLost();
    }
}

void EditorApp::onFocusGained()
{
    m_session.refreshResources();
    buildScripts(true);
}

void EditorApp::togglePlay()
{
    if (m_session.playing()) {
        m_session.stop();

        return;
    }
    if (m_playAfterBuild) {
        m_playAfterBuild = false;

        return;
    }
    // Тихо: если пересобирать нечего, лог остаётся пустым.
    if (buildScripts(true)) {
        m_playAfterBuild = true;
    } else {
        m_session.play();
    }
}

bool EditorApp::buildScripts(bool quiet)
{
    if (!m_session.project()) {
        return false;
    }
    const Project& project = *m_session.project();
    // Без единого .cpp собирать нечего: проект без скриптов не ждёт CMake при открытии.
    if (!hasGameSources(project.directory())) {
        if (!quiet) {
            log::info("The project has no C++ scripts yet: create one in Files");
        }

        return false;
    }
    auto setup = scriptBuildSetup(project, quiet);
    if (!setup) {
        return false;
    }
    m_scriptBuild.request(std::move(*setup));

    return true;
}

std::optional<ScriptBuildSetup> EditorApp::scriptBuildSetup(const Project& project, bool quiet) const
{
    if (!m_toolchain) {
        if (!quiet) {
            log::error("The engine SDK is missing in '{}': scripts cannot be built", toUtf8(sdkDirectory()));
        }

        return std::nullopt;
    }
    const ScriptConfiguration configuration = project.settings().get<ScriptsSettings>().configuration;
    // Переопределения - каждый раз заново: их меняют в настройках редактора без перезапуска.
    ScriptToolchain toolchain = *m_toolchain;
    applyOverrides(toolchain, m_settings->toolchain);

    return ScriptBuildSetup{
        .projectDirectory = project.directory(),
        .sdkDirectory = sdkDirectory(),
        .toolchain = std::move(toolchain),
        .configuration = std::string(enumName(configuration)),
        .quiet = quiet,
    };
}

void EditorApp::onUpdate(float deltaSeconds)
{
    m_log.collect();
    // Спрошено до takeResult: если сборки нет сейчас, взятый итог - последний,
    // новая сборка до конца кадра не начнётся.
    const bool buildSettled = !m_scriptBuild.building();
    // Собранное только что, или собранное другой конфигурацией, чем загружено.
    if (const auto built = m_scriptBuild.takeResult()) {
        m_scriptsFailed = !built->succeeded;
        if (built->succeeded && m_session.project() && (built->changed || built->library != m_session.gameLibrary())) {
            m_session.reloadGameModule(built->library);
        }
    }
    if (m_playAfterBuild && buildSettled) {
        m_playAfterBuild = false;
        if (m_scriptsFailed) {
            log::error("Play is cancelled: the scripts did not build, see the errors above");
        } else if (m_session.project()) {
            m_session.play();
        }
    }
    m_session.update(deltaSeconds, input());
    // Камера обновляется в окне сцены, там же известно, над ним ли курсор.
    m_deltaSeconds = deltaSeconds;
}

void EditorApp::onGui()
{
    ImGuizmo::BeginFrame();
    drawMainMenu({
        .host = m_host,
        .session = m_session,
        .actions = m_actions,
        .panels = m_panels,
        .languages = m_languages,
        .theme = m_theme,
        .settings = *m_settings,
        .log = m_log,
        .buildingScripts = m_scriptBuild.building(),
        .scriptsFailed = m_scriptsFailed,
        .showLog = [this] {
            m_logPanel->reveal();
        },
    });

    m_unsaved.draw(m_session);

    if (!m_session.project()) {
        if (auto project = m_projectManager->draw(m_host)) {
            openProject(std::move(*project));
        }

        return;
    }

    m_actions.handleShortcuts();

    // Закрытая развёрнутая панель возвращает раскладку.
    if (m_maximized && !m_maximized->visible()) {
        m_maximized = nullptr;
    }
    // Пока панель развёрнута, раскладку не показываем, но держим живой:
    // иначе пристыкованные окна выпали бы из неё.
    const ImGuiDockNodeFlags dockFlags = m_maximized ? ImGuiDockNodeFlags_KeepAliveOnly : ImGuiDockNodeFlags_None;
    const ImGuiID dockspace = ImGui::DockSpaceOverViewport(0, nullptr, dockFlags);
    if (!m_layoutChecked) {
        m_layoutChecked = true;
        buildDefaultLayout(dockspace);
    }

    EditorContext frame = context();
    for (const auto& panel : m_panels) {
        if (panel->visible() && (!m_maximized || m_maximized == panel.get())) {
            panel->draw(frame);
        }
    }
    if (!m_newWindowsDocked) {
        m_newWindowsDocked = dockNewWindows();
    }
    updateTitle();

    // Ничего не тянут и не печатают: следующая правка будет отдельным шагом отмены.
    if (!ImGui::IsAnyItemActive() && !ImGuizmo::IsUsing()) {
        m_session.finishEdit();
    }

    if (frame.mouseWrapping != m_mouseWrapping) {
        m_mouseWrapping = frame.mouseWrapping;
        setMouseWrapping(frame.mouseWrapping);
    }
}

void EditorApp::onCloseRequested()
{
    m_actions.run(actions::kQuit);
}

void EditorApp::openProject(Project project)
{
    const std::filesystem::path directory = project.directory();
    // Собранное в прошлый раз грузится сразу; сборка после открытия
    // перезагрузит модуль, только если исходники с тех пор менялись.
    const auto setup = scriptBuildSetup(project, true);
    const auto library = setup ? builtGameLibrary(*setup) : std::nullopt;
    m_session.open(std::move(project), library.value_or(std::filesystem::path()));
    EditorContext frame = context();
    for (const auto& panel : m_panels) {
        panel->onProjectOpened(frame);
    }
    updateTitle();

    m_projects->markOpened(directory);
    buildScripts(true);
}

void EditorApp::closeProject()
{
    m_scriptBuild.cancel();
    m_scriptsFailed = false;
    m_playAfterBuild = false;
    m_session.close();
    m_mouseWrapping = false;
    setMouseWrapping(false);
    updateTitle();
    m_projects->refresh();
}

void EditorApp::save()
{
    if (m_session.save()) {
        log::info("Project saved: {}", toUtf8(m_session.scenePath()));
    }
}

void EditorApp::openScene(const std::filesystem::path& path)
{
    if (path == m_session.scenePath()) {
        return;
    }
    // Настройки проекта при смене сцены не теряются: спрашиваем только про сцену.
    m_session.refreshModified();
    m_unsaved.request(m_session.sceneModified(), [this, path] {
        if (!m_session.openScene(path)) {
            log::error("Cannot open scene '{}'", toUtf8(path));
        }
    });
}

void EditorApp::updateTitle()
{
    setTitleDetail(windowTitle(m_session));
}

} // namespace yqlora::editor
