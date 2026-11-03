#include <doctest/doctest.h>

#include <yqlora/core/EngineTypes.hpp>
#include <yqlora/reflection/TypeRegistry.hpp>
#include <yqlora/render/Camera3D.hpp>
#include <yqlora/scene/Node3D.hpp>
#include <yqlora/scene/Scene.hpp>
#include <yqlora/scene/SceneSerializer.hpp>

#include <filesystem>

using namespace yqlora;

namespace {

// Компонент, которого нет в движке: так тесты проверяют типы, зарегистрированные снаружи.
struct TestComponent final : Component {
    YQLORA_TYPE(TestComponent, Component)

    int value = 100;
};

struct Fixture {
    Fixture()
    {
        registerEngineTypes(registry);
        registry.registerComponent<TestComponent>().property("value", &TestComponent::value);
    }

    TypeRegistry registry;
    SceneSerializer serializer{registry};
};

} // namespace

TEST_CASE_FIXTURE(Fixture, "SceneSerializer: scene survives save and load")
{
    Scene original{EngineServices::none()};
    auto& root = original.createRoot<Node>("Root");
    auto& player = root.addChild<Node3D>("Player");
    player.transform().setPosition({1.0f, 2.0f, 3.0f});
    player.transform().setRotationDegrees({0.0f, 100.0f, 0.0f});
    player.transform().setScale({2.0f, 2.0f, 2.0f});
    player.addComponent<TestComponent>().value = 42;
    player.getComponent<TestComponent>()->setEnabled(false);
    player.setActive(false);
    auto& camera = player.addChild<Camera3D>("Eyes");
    camera.setFovDegrees(75.0f);
    camera.setOrder(2);
    camera.setClear(CameraClear::DepthOnly);
    camera.setCullingMask(~LayerMask{0} << 4);
    camera.setBackground({0.1f, 0.2f, 0.3f, 1.0f});
    camera.setLayer(31);

    Scene loaded{EngineServices::none()};
    REQUIRE(serializer.fromJson(loaded, serializer.toJson(original)));

    REQUIRE(loaded.root());
    CHECK(loaded.root()->name() == "Root");
    CHECK(loaded.root()->id() == root.id());
    REQUIRE(loaded.root()->children().size() == 1);

    auto* loadedPlayer = dynamic_cast<Node3D*>(loaded.root()->children()[0].get());
    REQUIRE(loadedPlayer);
    CHECK(loadedPlayer->id() == player.id());
    CHECK(loadedPlayer->transform().position() == Vec3(1.0f, 2.0f, 3.0f));
    CHECK(loadedPlayer->transform().rotationDegrees() == Vec3(0.0f, 100.0f, 0.0f));
    CHECK(loadedPlayer->transform().scale() == Vec3(2.0f, 2.0f, 2.0f));
    // Transform не задвоился: встроенный компонент заполнен, а не добавлен ещё раз.
    CHECK(loadedPlayer->components().size() == 2);
    REQUIRE(loadedPlayer->getComponent<TestComponent>());
    CHECK(loadedPlayer->getComponent<TestComponent>()->value == 42);
    CHECK_FALSE(loadedPlayer->getComponent<TestComponent>()->enabled());
    CHECK_FALSE(loadedPlayer->active());
    CHECK(loaded.root()->active());

    auto* loadedCamera = dynamic_cast<Camera3D*>(loadedPlayer->findChild("Eyes"));
    REQUIRE(loadedCamera);
    CHECK(loadedCamera->fovDegrees() == doctest::Approx(75.0f));
    CHECK(loadedCamera->order() == 2);
    CHECK(loadedCamera->clear() == CameraClear::DepthOnly);
    // Старший бит маски тоже сохраняется: в файле она - int с теми же битами.
    CHECK(loadedCamera->cullingMask() == ~LayerMask{0} << 4);
    CHECK(loadedCamera->background() == Vec4(0.1f, 0.2f, 0.3f, 1.0f));
    CHECK(loadedCamera->layer() == 31);
}

TEST_CASE_FIXTURE(Fixture, "SceneSerializer: empty scene stays empty")
{
    Scene original{EngineServices::none()};
    Scene loaded{EngineServices::none()};
    loaded.createRoot<Node>();

    REQUIRE(serializer.fromJson(loaded, serializer.toJson(original)));
    CHECK(loaded.empty());
}

TEST_CASE_FIXTURE(Fixture, "SceneSerializer: broken file leaves the scene untouched")
{
    Scene scene{EngineServices::none()};
    auto& root = scene.createRoot<Node>("Keep");

    CHECK_FALSE(serializer.fromJson(scene, "not json"));
    CHECK_FALSE(serializer.fromJson(scene, R"({"version": 999, "root": null})"));
    CHECK_FALSE(serializer.fromJson(scene, R"({"version": 1, "root": {"type": "NoSuchNode"}})"));
    CHECK(scene.root() == &root);
}

TEST_CASE_FIXTURE(Fixture, "SceneSerializer: unknown node property is skipped, unknown component kept")
{
    Scene scene{EngineServices::none()};
    const char* json = R"({
        "version": 1,
        "root": {
            "type": "Node3D",
            "properties": {"name": "Box", "removedProperty": 5},
            "components": [
                {"type": "RemovedComponent", "properties": {}},
                {"type": "Transform3D", "properties": {"position": [0, 5, 0], "scale": "wrong"}}
            ]
        }
    })";

    REQUIRE(serializer.fromJson(scene, json));
    auto* root = dynamic_cast<Node3D*>(scene.root());
    REQUIRE(root);
    CHECK(root->name() == "Box");
    // Заглушка компонента пропавшего типа и Transform3D (см. test_missing_types).
    CHECK(root->components().size() == 2);
    CHECK(root->transform().position() == Vec3(0.0f, 5.0f, 0.0f));
    CHECK(root->transform().scale() == Vec3(1.0f));
}

TEST_CASE_FIXTURE(Fixture, "SceneSerializer: save and load a file")
{
    const auto file = std::filesystem::temp_directory_path() / "yqlora_tests" / "level.yscene";
    std::filesystem::create_directories(file.parent_path());

    Scene original{EngineServices::none()};
    original.createRoot<Node3D>("Level");
    REQUIRE(serializer.save(original, file));

    Scene loaded{EngineServices::none()};
    REQUIRE(serializer.load(loaded, file));
    REQUIRE(loaded.root());
    CHECK(loaded.root()->name() == "Level");

    CHECK_FALSE(serializer.load(loaded, file.parent_path() / "missing.yscene"));
}

namespace {

enum class Mood {
    Calm,
    Angry,
};

} // namespace

YQLORA_ENUM_NAMES(Mood, "Calm", "Angry")

namespace {

struct Guard final : Node3D {
    YQLORA_TYPE(Guard, Node3D)

    using Node3D::Node3D;

    Mood mood = Mood::Calm;
    NodeRef target;
    ResourceRef weapon;
};

} // namespace

TEST_CASE_FIXTURE(Fixture, "SceneSerializer: enums by name, node and resource references")
{
    registry.registerNode<Guard>()
        .property("mood", &Guard::mood)
        .property("target", &Guard::target)
        .property("weapon", &Guard::weapon);

    Scene original{EngineServices::none()};
    auto& root = original.createRoot<Node3D>("Root");
    auto& guard = root.addChild<Guard>("Guard");
    guard.mood = Mood::Angry;
    guard.target = NodeRef{root.id()};
    guard.weapon = ResourceRef{777};

    const std::string json = serializer.toJson(original);
    CHECK(json.find("\"Angry\"") != std::string::npos);

    Scene loaded{EngineServices::none()};
    REQUIRE(serializer.fromJson(loaded, json));
    auto* loadedGuard = dynamic_cast<Guard*>(loaded.root()->children()[0].get());
    REQUIRE(loadedGuard);
    CHECK(loadedGuard->mood == Mood::Angry);
    CHECK(loadedGuard->target.id == loaded.root()->id());
    CHECK(loadedGuard->weapon.id == 777);
}
