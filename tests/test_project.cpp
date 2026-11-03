#include "TestFiles.hpp"

#include <doctest/doctest.h>

#include <yqlora/core/EngineServices.hpp>
#include <yqlora/core/EngineTypes.hpp>
#include <yqlora/core/Version.hpp>
#include <yqlora/editor/EditorSession.hpp>
#include <yqlora/editor/ProjectList.hpp>
#include <yqlora/editor/ProjectSetup.hpp>
#include <yqlora/input/Input.hpp>
#include <yqlora/project/GeneralSettings.hpp>
#include <yqlora/project/Project.hpp>
#include <yqlora/project/ProjectUpgrade.hpp>
#include <yqlora/reflection/TypeRegistry.hpp>
#include <yqlora/render/Camera3D.hpp>
#include <yqlora/render/DirectionalLight3D.hpp>
#include <yqlora/scene/Scene.hpp>
#include <yqlora/scene/SceneSerializer.hpp>
#include <yqlora/scripting/GameModule.hpp>

#include <filesystem>
#include <fstream>

using namespace yqlora;
using namespace yqlora::editor;

namespace {

// Своя пустая папка на каждый тест, чтобы тесты не мешали друг другу.
} // namespace

TEST_CASE("Project: create makes a folder with the project file and a starter scene")
{
    TypeRegistry registry;
    registerEngineTypes(registry);
    const SceneSerializer serializer(registry);
    const auto location = test::freshDirectory("project_create");

    std::string error;
    const auto project = createProject(location, "My Game", serializer, error);
    REQUIRE(project);
    CHECK(project->directory() == location / "My Game");
    CHECK(std::filesystem::exists(project->projectFile()));

    const auto loaded = Project::open(project->directory(), registry);
    REQUIRE(loaded);
    CHECK(loaded->name() == "My Game");
    const auto& general = loaded->settings().get<GeneralSettings>();
    CHECK(general.startupScene == project->settings().get<GeneralSettings>().startupScene);
    CHECK(general.gameName == "My Game");
    REQUIRE(loaded->startupScenePath());

    Scene scene{EngineServices::none()};
    REQUIRE(serializer.load(scene, loaded->directory() / *loaded->startupScenePath()));
    REQUIRE(scene.root());
    CHECK(scene.root()->children().size() == 3);
    CHECK(dynamic_cast<Camera3D*>(scene.root()->findChild("Camera")) != nullptr);
    CHECK(dynamic_cast<DirectionalLight3D*>(scene.root()->findChild("Sun")) != nullptr);

    // Второй раз в ту же папку нельзя.
    CHECK_FALSE(createProject(location, "My Game", serializer, error));
    CHECK(error == "projects.error.exists");
}

TEST_CASE("Project: starter folders; C++ sources anywhere are files, not resources")
{
    TypeRegistry registry;
    registerEngineTypes(registry);
    const SceneSerializer serializer(registry);

    std::string error;
    const auto project = createProject(test::freshDirectory("project_sources"), "Game", serializer, error);
    REQUIRE(project);
    for (const char* folder : {"scenes", "scripts", "assets"}) {
        CHECK(std::filesystem::is_directory(project->directory() / folder));
    }
    REQUIRE(project->startupScenePath());
    CHECK(*project->startupScenePath() == std::filesystem::path("scenes") / "main.yscene");

    std::ofstream(project->directory() / "scripts" / "Player.cpp") << "// player";
    std::ofstream(project->directory() / "scenes" / "Player.hpp") << "// header next to a scene";
    const auto reopened = Project::open(project->directory(), registry);
    REQUIRE(reopened);
    CHECK(reopened->resources().id(std::filesystem::path("scripts") / "Player.cpp") == 0);
    CHECK(reopened->resources().id(std::filesystem::path("scenes") / "Player.hpp") == 0);
    CHECK_FALSE(std::filesystem::exists(project->directory() / "scripts" / "Player.cpp.meta"));
    CHECK(Project::isSource("x/Enemy.cpp"));
    CHECK_FALSE(Project::isSource("x/level.yscene"));
}

TEST_CASE("isValidFileName: names must be usable as file and folder names everywhere")
{
    CHECK(isValidFileName("Game"));
    CHECK(isValidFileName("Моя игра 2"));

    CHECK_FALSE(isValidFileName(""));
    CHECK_FALSE(isValidFileName(".."));
    CHECK_FALSE(isValidFileName("a/b"));
    CHECK_FALSE(isValidFileName("what?"));
    CHECK_FALSE(isValidFileName(" padded"));
    CHECK_FALSE(isValidFileName("dot."));
}

TEST_CASE("Project: a folder without a project file is not a project")
{
    const TypeRegistry registry;
    CHECK_FALSE(Project::open(test::freshDirectory("project_missing"), registry));
    CHECK_FALSE(Project::readInfo(test::freshDirectory("project_missing")));
}

TEST_CASE("ProjectList: recently opened first, saved between runs")
{
    const auto file = test::freshDirectory("project_list") / "projects.json";

    ProjectList list(file);
    list.add("/games/a");
    list.add("/games/b");
    list.add("/games/b/"); // тот же проект
    REQUIRE(list.entries().size() == 2);

    list.markOpened("/games/b");
    list.markOpened("/games/a");
    CHECK(list.entries()[0].directory == "/games/a");
    CHECK(list.entries()[1].directory == "/games/b");
    REQUIRE(list.save());

    ProjectList reloaded(file);
    reloaded.load();
    REQUIRE(reloaded.entries().size() == 2);
    CHECK(reloaded.entries()[0].directory == "/games/a");

    reloaded.remove("/games/a");
    CHECK(reloaded.entries().size() == 1);
}

TEST_CASE("ProjectList: missing file means an empty list")
{
    ProjectList list(test::freshDirectory("project_list_missing") / "projects.json");
    list.load();
    CHECK(list.entries().empty());
}

TEST_CASE("EditorSession: open, change, save and reopen a project")
{
    TypeRegistry registry;
    registerEngineTypes(registry);
    const SceneSerializer serializer(registry);
    std::string error;
    const auto project = createProject(test::freshDirectory("session"), "Game", serializer, error);
    REQUIRE(project);

    Input input;

    EngineServices services{input, registry};

    GameModule gameModule{registry};
    EditorSession session(services, serializer, gameModule);
    Scene& scene = session.scene();
    CHECK_FALSE(session.save()); // проект ещё не открыт

    REQUIRE(session.open(*Project::open(project->directory(), registry)));
    REQUIRE(scene.root());
    session.selection().select(scene.root()->id());
    scene.root()->setName("Renamed");
    REQUIRE(session.save());

    session.close();
    CHECK(scene.empty());
    CHECK_FALSE(session.project());
    CHECK(session.selection().selected() == 0);

    REQUIRE(session.open(*Project::open(project->directory(), registry)));
    CHECK(scene.root()->name() == "Renamed");
}

TEST_CASE("EditorSession: a project with a broken scene opens with an empty scene")
{
    TypeRegistry registry;
    registerEngineTypes(registry);
    const SceneSerializer serializer(registry);
    std::string error;
    const auto project = createProject(test::freshDirectory("session_broken"), "Game", serializer, error);
    REQUIRE(project);
    std::filesystem::remove(project->directory() / *project->startupScenePath());

    Input input;

    EngineServices services{input, registry};

    GameModule gameModule{registry};
    EditorSession session(services, serializer, gameModule);
    Scene& scene = session.scene();
    scene.createRoot<Node>("FromBefore");
    CHECK_FALSE(session.open(*Project::open(project->directory(), registry)));
    CHECK(session.project());
    CHECK(scene.empty());
}

TEST_CASE("deleteProjectFolder: removes only real project folders")
{
    TypeRegistry registry;
    registerEngineTypes(registry);
    const SceneSerializer serializer(registry);
    const auto location = test::freshDirectory("project_delete");
    std::string error;
    const auto project = createProject(location, "Game", serializer, error);
    REQUIRE(project);

    const auto notProject = location / "Photos";
    std::filesystem::create_directories(notProject);
    CHECK_FALSE(deleteProjectFolder(notProject, error));
    CHECK(error == "projects.error.notProject");
    CHECK(std::filesystem::exists(notProject));

    CHECK(deleteProjectFolder(project->directory(), error));
    CHECK_FALSE(std::filesystem::exists(project->directory()));
}

TEST_CASE("Project: the version tells whether a project opens, needs an upgrade or is too new")
{
    TypeRegistry registry;
    registerEngineTypes(registry);
    const SceneSerializer serializer(registry);
    std::string error;
    const auto project = createProject(test::freshDirectory("project_version"), "Game", serializer, error);
    REQUIRE(project);

    const auto info = Project::readInfo(project->directory());
    REQUIRE(info);
    CHECK(info->name == "Game");
    CHECK(info->formatVersion == Project::formatVersion());
    CHECK(info->engineVersion == engineVersion());
    CHECK(info->compatibility == FormatCompatibility::Current);

    const auto rewrite = [&](const char* text) {
        std::ofstream(project->projectFile()) << text;
    };
    rewrite(R"({"name": "Game", "version": 99, "engine": "9.9.9"})");
    CHECK(Project::readInfo(project->directory())->compatibility == FormatCompatibility::Newer);
    CHECK(Project::readInfo(project->directory())->engineVersion == "9.9.9");
    CHECK_FALSE(Project::open(project->directory(), registry));
    CHECK_FALSE(upgradeProject(project->directory(), serializer));

    // Без версии - файл не этого формата, переводить не с чего.
    rewrite(R"({"name": "Game"})");
    CHECK(Project::readInfo(project->directory())->compatibility == FormatCompatibility::Unsupported);
    CHECK_FALSE(Project::open(project->directory(), registry));
}

TEST_CASE("Project: upgrading a current project rewrites it and keeps it working")
{
    TypeRegistry registry;
    registerEngineTypes(registry);
    const SceneSerializer serializer(registry);
    std::string error;
    const auto project = createProject(test::freshDirectory("project_upgrade"), "Game", serializer, error);
    REQUIRE(project);

    REQUIRE(upgradeProject(project->directory(), serializer));
    const auto reopened = Project::open(project->directory(), registry);
    REQUIRE(reopened);
    CHECK(
        reopened->settings().get<GeneralSettings>().startupScene
        == project->settings().get<GeneralSettings>().startupScene
    );

    // Испорченный файл раздела настроек: перевод отказывается и ничего не трогает.
    std::ofstream(project->directory() / "settings" / "broken.json") << "not json";
    CHECK_FALSE(upgradeProject(project->directory(), serializer));
    std::filesystem::remove(project->directory() / "settings" / "broken.json");
    REQUIRE(upgradeProject(project->directory(), serializer));

    // Испорченная сцена: перевод отказывается и ничего не трогает.
    std::ofstream(project->directory() / "broken.yscene") << "not json";
    CHECK_FALSE(upgradeProject(project->directory(), serializer));
    CHECK(Project::open(project->directory(), registry));
}
