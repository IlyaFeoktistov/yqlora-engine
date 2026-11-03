#include <doctest/doctest.h>

#include <yqlora/core/EngineServices.hpp>
#include <yqlora/core/EngineTypes.hpp>
#include <yqlora/editor/EditorSession.hpp>
#include <yqlora/editor/ProjectFiles.hpp>
#include <yqlora/editor/ProjectSetup.hpp>
#include <yqlora/input/Input.hpp>
#include <yqlora/project/Project.hpp>
#include <yqlora/reflection/TypeRegistry.hpp>
#include <yqlora/render/Camera3D.hpp>
#include <yqlora/render/DirectionalLight3D.hpp>
#include <yqlora/scene/Scene.hpp>
#include <yqlora/scene/SceneSerializer.hpp>
#include <yqlora/scripting/GameModule.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>

using namespace yqlora;
using namespace yqlora::editor;

namespace {

struct Fixture {
    Fixture()
    {
        registerEngineTypes(registry);
        const auto location = std::filesystem::temp_directory_path() / "yqlora_tests" / "files";
        std::filesystem::remove_all(location);
        std::filesystem::create_directories(location);
        std::string error;
        project = *createProject(location, "Game", serializer, error);
    }

    TypeRegistry registry;
    SceneSerializer serializer{registry};
    Project project;
    ProjectFiles files{project};
};

std::vector<std::string> names(const ProjectFiles::Entry& entry)
{
    std::vector<std::string> result;
    for (const auto& child : entry.children) {
        result.push_back(child.name);
    }

    return result;
}

} // namespace

TEST_CASE_FIXTURE(Fixture, "ProjectFiles: folders first, the project file, settings and .meta are hidden")
{
    REQUIRE(files.createFolder({}, files.suggestName(*findNewResourceType(kFolderKind), {})));
    REQUIRE(
        files.createScene({}, files.suggestName(*findNewResourceType(SceneSerializer::kResourceKind), {}), serializer)
    );

    const auto root = files.scan();
    CHECK(names(root) == std::vector<std::string>{"New Folder", "assets", "scenes", "scripts", "New Scene.yscene"});
    CHECK(root.children[0].directory);
}

TEST_CASE_FIXTURE(Fixture, "ProjectFiles: new resources get free suggested names, taken names are refused")
{
    CHECK(files.suggestName(*findNewResourceType(kCppScriptKind), "scripts") == "NewScript");
    const auto first = files.createScene(
        {},
        files.suggestName(*findNewResourceType(SceneSerializer::kResourceKind), {}),
        serializer
    );
    REQUIRE(first);
    CHECK(*first == "New Scene.yscene");
    CHECK(files.suggestName(*findNewResourceType(SceneSerializer::kResourceKind), {}) == "New Scene 2");
    CHECK_FALSE(files.createScene({}, "New Scene", serializer));
    CHECK_FALSE(files.createScene({}, "bad/name", serializer));
    CHECK_FALSE(files.createFolder({}, "settings"));
    CHECK_FALSE(files.createFolder({}, ""));
    CHECK(files.createFolder("scenes", "levels") == std::filesystem::path("scenes") / "levels");
    CHECK_FALSE(files.createFolder("scenes", "levels"));
}

TEST_CASE_FIXTURE(Fixture, "ProjectFiles: a new scene can be opened")
{
    const auto second = files.createScene({}, "Level", serializer);
    REQUIRE(second);
    CHECK(*second == "Level.yscene");

    Input input;

    EngineServices services{input, registry};

    GameModule gameModule{registry};
    EditorSession session(services, serializer, gameModule);
    Scene& scene = session.scene();
    REQUIRE(session.open(*Project::open(project.directory(), registry)));
    REQUIRE(session.openScene(*second));
    CHECK(session.document().id() == project.resources().id(*second));
    CHECK(session.scenePath() == *second);
    // Новая сцена уже с камерой и светом, корень назван по сцене.
    REQUIRE(scene.root());
    CHECK(scene.root()->name() == "Level");
    CHECK(dynamic_cast<Camera3D*>(scene.root()->findChild("Camera")) != nullptr);
    CHECK(dynamic_cast<DirectionalLight3D*>(scene.root()->findChild("Sun")) != nullptr);
    const std::size_t nodes = scene.root()->children().size();

    // Сохраняется в открытую сцену, а не в главную.
    session.editing().createNode(*registry.find("Node3D"), scene.root());
    REQUIRE(session.save());
    Scene check{EngineServices::none()};
    REQUIRE(serializer.load(check, project.directory() / *second));
    REQUIRE(check.root());
    CHECK(check.root()->children().size() == nodes + 1);
}

TEST_CASE_FIXTURE(Fixture, "ProjectFiles: nothing outside the project can be touched")
{
    CHECK_FALSE(files.canModify(""));
    CHECK_FALSE(files.canModify("project.yqlora"));
    CHECK_FALSE(files.canModify("settings/general.json"));
    CHECK_FALSE(files.canModify("../other"));
    CHECK_FALSE(files.canModify("a/../../other"));
    CHECK(files.canModify("scenes/main.yscene"));

    CHECK_FALSE(files.remove("../files"));
    CHECK(std::filesystem::exists(project.directory()));
    CHECK_FALSE(files.rename("scenes/main.yscene", "bad/name.yscene"));
}

TEST_CASE_FIXTURE(Fixture, "ProjectFiles: a renamed scene keeps its id, the session and project follow it")
{
    Input input;
    EngineServices services{input, registry};
    GameModule gameModule{registry};
    EditorSession session(services, serializer, gameModule);
    REQUIRE(session.open(*Project::open(project.directory(), registry)));
    const ResourceId id = session.document().id();
    REQUIRE(id != 0);

    // Сессия работает со своим экземпляром проекта: файлы меняем через него.
    ProjectFiles sessionFiles(*session.project());
    const auto folder = sessionFiles.createFolder({}, "Old");
    REQUIRE(folder);
    const auto moved = sessionFiles.rename("scenes/main.yscene", "level.yscene");
    REQUIRE(moved);
    const auto level = std::filesystem::path("scenes") / "level.yscene";
    CHECK(session.document().id() == id);
    CHECK(session.scenePath() == level);
    CHECK(session.project()->startupScenePath() == level);
    CHECK(std::filesystem::exists(project.directory() / "scenes" / "level.yscene.meta"));
    CHECK(Project::open(project.directory(), registry)->startupScenePath() == level);

    CHECK(sessionFiles.remove(*folder));
    CHECK_FALSE(std::filesystem::exists(project.directory() / *folder));
}

TEST_CASE_FIXTURE(Fixture, "ProjectFiles: a C++ script from the template, a file and not a resource")
{
    const auto created = files.createScript("scripts", "Player", YQLORA_SCRIPT_TEMPLATE_DIR);
    REQUIRE(created);
    CHECK(*created == std::filesystem::path("scripts") / "Player.cpp");
    std::ifstream header(project.directory() / "scripts" / "Player.hpp");
    const std::string text((std::istreambuf_iterator<char>(header)), std::istreambuf_iterator<char>());
    CHECK(text.find("class Player : public yqlora::Script") != std::string::npos);
    CHECK(text.find("$SCRIPT_NAME$") == std::string::npos);
    CHECK_FALSE(std::filesystem::exists(project.directory() / "scripts" / "Player.cpp.meta"));
    CHECK(project.resources().id(*created) == 0);

    // Имя занято, не имя класса, папка вне проекта - ничего не создаётся.
    CHECK_FALSE(files.createScript("scripts", "Player", YQLORA_SCRIPT_TEMPLATE_DIR));
    CHECK_FALSE(files.createScript("scripts", "2Fast", YQLORA_SCRIPT_TEMPLATE_DIR));
    CHECK_FALSE(files.createScript("scripts", "Bad Name", YQLORA_SCRIPT_TEMPLATE_DIR));
    CHECK_FALSE(files.createScript("../outside", "Thief", YQLORA_SCRIPT_TEMPLATE_DIR));
    CHECK(isValidScriptName("Enemy_2"));
    CHECK_FALSE(isValidScriptName("Враг"));

    // Шаблона нет - нет и половины файлов.
    CHECK_FALSE(files.createScript("scripts", "Lost", project.directory() / "no-template"));
    CHECK_FALSE(std::filesystem::exists(project.directory() / "scripts" / "Lost.hpp"));
}

TEST_CASE_FIXTURE(Fixture, "newResourceTypes: every kind is created from its row, and its name is then taken")
{
    const NewResourceContext context{serializer, YQLORA_ASSETS_DIR};
    for (const NewResourceType& type : newResourceTypes()) {
        CAPTURE(type.kind);
        CHECK(findNewResourceType(type.kind) == &type);
        const std::string name = files.suggestName(type, "scripts");
        CHECK(name == type.defaultName);
        const auto created = type.create(files, "scripts", name, context);
        REQUIRE(created);
        CHECK(std::filesystem::exists(project.directory() / *created));
        CHECK(std::filesystem::is_directory(project.directory() / *created) == type.extension.empty());
        CHECK(files.suggestName(type, "scripts") != name);
        CHECK_FALSE(type.create(files, "scripts", name, context));
    }
    CHECK_FALSE(findNewResourceType("unknown"));
}
