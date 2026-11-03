#include "TestFiles.hpp"

#include <doctest/doctest.h>

#include <yqlora/core/EngineTypes.hpp>
#include <yqlora/editor/PlayMode.hpp>
#include <yqlora/editor/ProjectSetup.hpp>
#include <yqlora/editor/SceneDocument.hpp>
#include <yqlora/project/GeneralSettings.hpp>
#include <yqlora/project/Project.hpp>
#include <yqlora/reflection/TypeRegistry.hpp>
#include <yqlora/resource/ResourceCache.hpp>
#include <yqlora/scene/Node3D.hpp>
#include <yqlora/scene/Scene.hpp>
#include <yqlora/scene/SceneSerializer.hpp>

#include <filesystem>
#include <fstream>

using namespace yqlora;
using namespace yqlora::editor;

namespace {

struct Fixture {
    Fixture()
    {
        registerEngineTypes(registry);
    }

    TypeRegistry registry;
    SceneSerializer serializer{registry};
    Scene scene{EngineServices::none()};
};

} // namespace

TEST_CASE_FIXTURE(Fixture, "PlayMode: the game runs in a copy that is thrown away on stop")
{
    scene.createRoot<Node3D>("Root");
    PlayMode play{serializer};
    CHECK_FALSE(play.playing());

    REQUIRE(play.start(scene));
    CHECK_FALSE(play.start(scene));
    REQUIRE(play.scene());
    CHECK(play.scene() != &scene);
    CHECK(play.scene()->root()->id() == scene.root()->id());
    // Свой ввод, но те же загруженные ресурсы.
    CHECK(&play.scene()->services().input() != &scene.services().input());
    CHECK(&play.scene()->services().resources() == &scene.services().resources());

    play.scene()->root()->setName("Changed");
    CHECK(scene.root()->name() == "Root");

    play.setPaused(true);
    CHECK_FALSE(play.running());
    play.stop();
    CHECK_FALSE(play.playing());
    CHECK_FALSE(play.paused());
    CHECK(play.scene() == nullptr);

    // Без игры пауза не включается.
    play.setPaused(true);
    CHECK_FALSE(play.paused());
}

TEST_CASE_FIXTURE(Fixture, "SceneDocument: a scene that cannot be read leaves the current one")
{
    const auto location = test::freshDirectory("document");
    std::string error;
    Project project = *createProject(location, "Game", serializer, error);

    SceneDocument document{EngineServices::none(), serializer};
    const Scene& documentScene = document.scene();
    const ResourceId startup = project.settings().get<GeneralSettings>().startupScene.id;
    REQUIRE(document.load(project, startup));
    CHECK(document.id() == startup);
    const NodeId root = documentScene.root()->id();

    // Файл, который не читается: сцена и её ID остаются прежними.
    const auto broken = project.directory() / "broken.yscene";
    std::ofstream(broken) << "not json";
    project.resources().scan(project.directory(), Project::reservedNames());
    const ResourceId brokenId = project.resources().id("broken.yscene");
    REQUIRE(brokenId != 0);
    CHECK_FALSE(document.load(project, brokenId));
    CHECK(document.id() == startup);
    CHECK(documentScene.root()->id() == root);

    document.reset(0);
    CHECK(documentScene.empty());
    CHECK(document.path(project).empty());
}
