#include "TestFiles.hpp"

#include <doctest/doctest.h>

#include <yqlora/core/EngineTypes.hpp>
#include <yqlora/editor/ProjectManager.hpp>
#include <yqlora/reflection/TypeRegistry.hpp>
#include <yqlora/scene/SceneSerializer.hpp>

#include <filesystem>
#include <fstream>

using namespace yqlora;
using namespace yqlora::editor;

namespace {

// Пустая папка теста. Чистится до того, как менеджер прочитает в ней список проектов.
std::filesystem::path freshFolder()
{
    const auto folder = test::freshDirectory("manager");

    return folder;
}

struct Fixture {
    Fixture()
    {
        registerEngineTypes(registry);
    }

    const std::filesystem::path root = freshFolder();
    TypeRegistry registry;
    SceneSerializer serializer{registry};
    ProjectManager manager{root / "projects.json", serializer};
    std::string error;
};

} // namespace

TEST_CASE_FIXTURE(Fixture, "ProjectManager: create, open and remove")
{
    CHECK(manager.rows().empty());
    REQUIRE(manager.create(root, "Game", error));
    REQUIRE(manager.rows().size() == 1);
    CHECK(manager.rows()[0].name == "Game");
    CHECK(manager.rows()[0].compatibility == FormatCompatibility::Current);

    const auto opened = manager.open(0);
    CHECK(opened.status == ProjectManager::OpenStatus::Opened);
    REQUIRE(opened.project);
    CHECK(opened.project->name() == "Game");

    // Без удаления папки проект остаётся на диске.
    REQUIRE(manager.remove(0, false, error));
    CHECK(manager.rows().empty());
    CHECK(std::filesystem::exists(root / "Game" / Project::fileName()));

    REQUIRE(manager.import(root / "Game", error));
    REQUIRE(manager.rows().size() == 1);
    REQUIRE(manager.remove(0, true, error));
    CHECK_FALSE(std::filesystem::exists(root / "Game"));
}

TEST_CASE_FIXTURE(Fixture, "ProjectManager: a folder that is not a project is not imported")
{
    std::filesystem::create_directories(root / "Empty");
    CHECK_FALSE(manager.import(root / "Empty", error));
    CHECK(error == "projects.error.notProject");
    CHECK(manager.rows().empty());
}

TEST_CASE_FIXTURE(Fixture, "ProjectManager: a project removed from disk is marked missing")
{
    REQUIRE(manager.create(root, "Game", error));
    std::filesystem::remove_all(root / "Game");

    const auto opened = manager.open(0);
    CHECK(opened.status == ProjectManager::OpenStatus::Missing);
    CHECK(manager.rows()[0].missing);
}

TEST_CASE_FIXTURE(Fixture, "ProjectManager: a project from a newer engine is not opened")
{
    REQUIRE(manager.create(root, "Game", error));
    const auto file = root / "Game" / Project::fileName();
    std::string text;
    {
        std::ifstream in(file);
        text.assign(std::istreambuf_iterator<char>(in), {});
    }
    const auto at = text.find("\"version\"");
    REQUIRE(at != std::string::npos);
    const auto value = text.find_first_of("0123456789", at);
    const auto end = text.find_first_not_of("0123456789", value);
    text.replace(value, end - value, "999");
    std::ofstream(file) << text;
    manager.refresh();

    CHECK(manager.open(0).status == ProjectManager::OpenStatus::CannotOpen);
}
