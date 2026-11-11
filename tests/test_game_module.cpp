#include "EngineRegistry.hpp"

#include <doctest/doctest.h>

#include <yqlora/core/EngineServices.hpp>
#include <yqlora/editor/EditorSession.hpp>
#include <yqlora/editor/Inspection.hpp>
#include <yqlora/editor/ModuleCopies.hpp>
#include <yqlora/editor/ProjectSetup.hpp>
#include <yqlora/input/Input.hpp>
#include <yqlora/project/Project.hpp>
#include <yqlora/reflection/ObjectAccess.hpp>
#include <yqlora/scene/MissingComponent.hpp>
#include <yqlora/scene/Node3D.hpp>
#include <yqlora/scene/Scene.hpp>
#include <yqlora/scene/SceneSerializer.hpp>
#include <yqlora/scripting/GameModule.hpp>

#include <algorithm>
#include <array>
#include <filesystem>
#include <string>

using namespace yqlora;
using namespace yqlora::editor;

namespace {

constexpr std::string_view kScript = "TestMover";

struct Fixture {
    // Скрипт модуля на корне сцены, как его добавляет редактор: по имени типа.
    Component& addScript(Scene& scene, std::string name) const
    {
        auto& root = scene.createRoot<Node3D>(std::move(name));

        return root.addComponent(registry.find(kScript)->createComponent());
    }

    const Property& property(std::string_view name) const
    {
        const auto properties = registry.properties(*registry.find(kScript));

        return **std::ranges::find(properties, name, &Property::name);
    }

    test::EngineRegistry registry;
    Input input;
    EngineServices services{input, registry};
    GameModule module{registry};
};

} // namespace

TEST_CASE_FIXTURE(Fixture, "GameModule: a missing library is not loaded")
{
    CHECK_FALSE(module.load(std::filesystem::path(YQLORA_TEST_GAME_MODULE).parent_path() / "missing-game-module"));
    CHECK_FALSE(module.loaded());
}

TEST_CASE_FIXTURE(Fixture, "GameModule: load, register, create, run, destroy, unload and load again")
{
    REQUIRE(module.load(YQLORA_TEST_GAME_MODULE));
    CHECK(module.rejected().empty());
    const TypeInfo* type = registry.find(kScript);
    REQUIRE(type);
    CHECK(type->module() == GameModule::moduleName());
    CHECK(type->isScript());

    // Сцена редактора и сцена Play, в обеих экземпляры типа из библиотеки.
    Scene edited{services};
    Scene played{services};
    Component& editedScript = addScript(edited, "Edited");
    Component& playedScript = addScript(played, "Played");
    CHECK(property("speed").set(editedScript, PropertyValue{3.0f}));
    played.advance(0.1f);
    played.advance(0.1f);
    CHECK(property("updates").get(playedScript) == PropertyValue{2});
    CHECK(played.root()->name() == "Played+start");

    const std::array<Scene*, 2> scenes{&edited, &played};
    module.unload(scenes);
    CHECK_FALSE(module.loaded());
    CHECK(registry.find(kScript) == nullptr);
    CHECK(edited.componentsOfType(kScript).empty());
    CHECK(played.componentsOfType(kScript).empty());
    // onStop пришёл, пока код был загружен; незапущенный скрипт его не получает.
    CHECK(played.root()->name() == "Played+start+stop");
    CHECK(edited.root()->name() == "Edited");

    // Как после пересборки: тот же модуль загружается снова.
    REQUIRE(module.load(YQLORA_TEST_GAME_MODULE));
    CHECK(registry.find(kScript));
    addScript(edited, "Again");
    module.unload(scenes);
}

TEST_CASE_FIXTURE(
    Fixture,
    "GameModule: a script registers methods and events; a listener calls a method of another script"
)
{
    REQUIRE(module.load(YQLORA_TEST_GAME_MODULE));
    CHECK(module.rejected().empty());
    const TypeInfo* door = registry.find("TestDoor");
    REQUIRE(door);
    const auto methods = registry.methods(*door);
    CHECK(std::ranges::find(methods, "open", &Method::name) != methods.end());
    CHECK(std::ranges::find(methods, "add", &Method::name) != methods.end());
    const auto events = registry.events(*door);
    REQUIRE(events.size() == 1);
    CHECK(events.front()->name == "onOpened");

    Scene scene{services};
    scene.setPlaying(true);
    auto& root = scene.createRoot<Node3D>("Root");
    auto& button = root.addChild<Node3D>("Button");
    auto& lamp = root.addChild<Node3D>("Lamp");
    Component& pressed = button.addComponent(door->createComponent());
    Component& listening = lamp.addComponent(door->createComponent());
    // Подписчик - как его задаёт инспектор: элемент списка свойства события.
    const PropertyValue target = RecordValue{{ComponentRef<>{lamp.id(), listening.id()}, std::string("add")}};
    REQUIRE(setProperty(pressed, "onOpened", ListValue{{target}}, registry));

    CHECK(callMethod(pressed, "open", {}, registry).called);
    CHECK(getProperty(pressed, "opened", registry) == PropertyValue{1});
    CHECK(getProperty(listening, "opened", registry) == PropertyValue{1});

    const std::array<Scene*, 1> scenes{&scene};
    module.unload(scenes);
}

TEST_CASE_FIXTURE(Fixture, "EditorSession: the game library of the project loads on open and unloads on close")
{
    const auto location = std::filesystem::temp_directory_path() / "yqlora_tests" / "session_game_module";
    std::filesystem::remove_all(location);
    SceneSerializer serializer{registry};
    std::string error;
    const auto created = createProject(location, "Game", serializer, error);
    REQUIRE(created);
    const std::filesystem::path library = YQLORA_TEST_GAME_MODULE;

    EditorSession session{services, serializer, module};
    REQUIRE(session.open(*Project::open(created->directory(), registry), library));
    CHECK(session.gameLibrary() == library);
    REQUIRE(module.loaded());
    const TypeInfo* type = registry.find(kScript);
    REQUIRE(type);

    // Скрипты - своей группой в "Добавить компонент".
    Node& root = *session.scene().root();
    session.selection().select(root.id());
    const auto inspection = inspect(session);
    REQUIRE(inspection);
    CHECK(std::ranges::find(inspection->addableScripts, type) != inspection->addableScripts.end());
    CHECK(std::ranges::find(inspection->addable, type) == inspection->addable.end());

    // Один скрипт остаётся в сцене, второй снят и лежит в истории правок:
    // оба уничтожаются до выгрузки библиотеки.
    REQUIRE(session.editing().addComponent(root, *type));
    Component* removed = session.editing().addComponent(root, *type);
    REQUIRE(removed);
    REQUIRE(session.editing().removeComponent(root, *removed));
    REQUIRE(session.save());
    session.close();
    CHECK_FALSE(module.loaded());
    CHECK(registry.find(kScript) == nullptr);

    // Снова открытый проект: скрипт из файла настоящий, не заглушка.
    REQUIRE(session.open(*Project::open(created->directory(), registry), library));
    CHECK(session.scene().componentsOfType(kScript).size() == 1);

    // Без библиотеки проект открывается, скрипт ждёт её заглушкой.
    session.close();
    REQUIRE(session.open(*Project::open(created->directory(), registry)));
    CHECK_FALSE(module.loaded());
    CHECK(std::ranges::any_of(session.scene().root()->components(), [](const auto& component) {
        return dynamic_cast<const MissingComponent*>(component.get()) != nullptr;
    }));
}

namespace {

// Проект с одним скриптом модуля на корне сцены, открытый с тестовой библиотекой.
struct ReloadFixture : Fixture {
    SceneSerializer serializer{registry};
    EditorSession session{services, serializer, module};
    std::filesystem::path directory;

    explicit ReloadFixture(const char* name)
    {
        const auto location = std::filesystem::temp_directory_path() / "yqlora_tests" / name;
        std::filesystem::remove_all(location);
        std::string error;
        const auto created = createProject(location, "Game", serializer, error);
        REQUIRE(created);
        directory = created->directory();
        REQUIRE(session.open(*Project::open(directory, registry), YQLORA_TEST_GAME_MODULE));
    }

    std::size_t copies() const
    {
        const auto reload = ModuleCopies(directory).directory();

        return static_cast<std::size_t>(
            std::distance(std::filesystem::directory_iterator(reload), std::filesystem::directory_iterator())
        );
    }
};

} // namespace

TEST_CASE("EditorSession: hot reload keeps script values, the selection and the file; history is cleared")
{
    ReloadFixture fixture("hot_reload");
    auto& session = fixture.session;
    Node& root = *session.scene().root();
    const NodeId rootId = root.id();
    Component* script = session.editing().addComponent(root, *fixture.registry.find(kScript));
    REQUIRE(script);
    REQUIRE(session.save());
    fixture.property("speed").set(*script, PropertyValue(4.5f));
    session.selection().select(rootId);
    CHECK(session.canUndo());
    const bool modifiedBefore = session.sceneModified();

    for (int reload = 0; reload < 3; ++reload) {
        REQUIRE(session.reloadGameModule(YQLORA_TEST_GAME_MODULE));
    }
    CHECK(fixture.module.loaded());
    CHECK_FALSE(session.canUndo());
    CHECK(session.selection().selected() == rootId);
    CHECK(session.sceneModified() == modifiedBefore);
    const auto live = session.scene().componentsOfType(kScript);
    REQUIRE(live.size() == 1);
    CHECK(std::get<float>(fixture.property("speed").get(*live.front())) == doctest::Approx(4.5f));
    // Загружена одна копия, остальные удалены.
    CHECK(fixture.copies() == 1);

    session.close();
    CHECK(fixture.copies() == 0);
}

TEST_CASE("EditorSession: a reload that fails keeps the scripts as stubs with their data")
{
    ReloadFixture fixture("hot_reload_failed");
    auto& session = fixture.session;
    Node& root = *session.scene().root();
    Component* script = session.editing().addComponent(root, *fixture.registry.find(kScript));
    REQUIRE(script);
    fixture.property("speed").set(*script, PropertyValue(7.0f));

    CHECK_FALSE(session.reloadGameModule(YQLORA_THROWING_GAME_MODULE));
    CHECK_FALSE(fixture.module.loaded());
    CHECK(session.gameLibrary().empty());
    // Ни одного типа модуля: ни тестового скрипта, ни того, что успел
    // зарегистрировать сломанный модуль до исключения.
    CHECK(fixture.registry.find(kScript) == nullptr);
    CHECK(fixture.registry.find("Registered") == nullptr);
    CHECK(session.scene().componentsOfType(kScript).empty());

    REQUIRE(session.reloadGameModule(YQLORA_TEST_GAME_MODULE));
    const auto live = session.scene().componentsOfType(kScript);
    REQUIRE(live.size() == 1);
    CHECK(std::get<float>(fixture.property("speed").get(*live.front())) == doctest::Approx(7.0f));
}

TEST_CASE_FIXTURE(Fixture, "GameModule: an entry function that throws leaves no types behind")
{
    CHECK_FALSE(module.load(YQLORA_THROWING_GAME_MODULE));
    CHECK_FALSE(module.loaded());
    CHECK(registry.find("Registered") == nullptr);
    CHECK(module.load(YQLORA_TEST_GAME_MODULE));
    module.unload({});
}

#ifdef YQLORA_EXAMPLE_SCRIPTS_MODULE
TEST_CASE_FIXTURE(Fixture, "GameModule: the example Rotator turns its node in play")
{
    REQUIRE(module.load(YQLORA_EXAMPLE_SCRIPTS_MODULE));
    const TypeInfo* type = registry.find("Rotator");
    REQUIRE(type);
    CHECK(type->requiredNode() == "Node3D");

    Scene scene{services};
    auto& root = scene.createRoot<Node3D>("Spinning");
    root.addComponent(type->createComponent());
    const Quat before = root.transform().rotation();
    scene.advance(0.5f);
    CHECK(root.transform().rotation() != before);

    const std::array<Scene*, 1> scenes{&scene};
    module.unload(scenes);
}
#endif
