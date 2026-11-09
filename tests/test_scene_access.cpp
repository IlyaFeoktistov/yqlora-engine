#include "EngineRegistry.hpp"

#include <doctest/doctest.h>

#include <yqlora/core/EngineServices.hpp>
#include <yqlora/input/Input.hpp>
#include <yqlora/reflection/ComponentLookup.hpp>
#include <yqlora/reflection/ComponentRules.hpp>
#include <yqlora/reflection/Method.hpp>
#include <yqlora/reflection/ObjectAccess.hpp>
#include <yqlora/reflection/Property.hpp>
#include <yqlora/reflection/TypeRegistry.hpp>
#include <yqlora/render/Camera3D.hpp>
#include <yqlora/render/DirectionalLight3D.hpp>
#include <yqlora/render/Environment.hpp>
#include <yqlora/render/FrameBuilder.hpp>
#include <yqlora/render/RenderSystem.hpp>
#include <yqlora/resource/BuiltinResources.hpp>
#include <yqlora/scene/Node3D.hpp>
#include <yqlora/scene/Scene.hpp>
#include <yqlora/scene/SceneSerializer.hpp>
#include <yqlora/scripting/Script.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace yqlora;

namespace {

struct Weapon : Component {
    YQLORA_TYPE(Weapon, Component)
};

struct Sword final : Weapon {
    YQLORA_TYPE(Sword, Weapon)
};

struct Shield final : Component {
    YQLORA_TYPE(Shield, Component)
};

// Пишет, какие вызовы жизненного цикла получил; Destroyer уничтожает цель.
struct Journal : Component {
    YQLORA_TYPE(Journal, Component)

    explicit Journal(std::vector<std::string>* log = nullptr, std::string tag = {})
        : log(log)
        , tag(std::move(tag))
    {
    }

    void onUpdate(float) override
    {
        write("update");
    }

    void onLateUpdate(float) override
    {
        write("late");
    }

    void onDisable() override
    {
        write("disable");
    }

    void onStop() override
    {
        write("stop");
    }

    void write(const std::string& what) const
    {
        if (log) {
            log->push_back(tag + " " + what);
        }
    }

    std::vector<std::string>* log;
    std::string tag;
};

struct Destroyer final : Journal {
    YQLORA_TYPE(Destroyer, Journal)

    using Journal::Journal;

    void onUpdate(float delta) override
    {
        Journal::onUpdate(delta);
        if (target) {
            owner()->scene()->destroy(*std::exchange(target, nullptr));
        }
        if (node) {
            owner()->scene()->destroy(*std::exchange(node, nullptr));
        }
    }

    Component* target = nullptr;
    Node* node = nullptr;
};

// Требует Shield на своём узле.
struct Grip final : Component {
    YQLORA_TYPE(Grip, Component)
};

// Скрипт смены дня и ночи: меняет небо, солнце и камеру через систему рендера.
struct DayNight final : Script {
    YQLORA_TYPE(DayNight, Script)

    void onUpdate(float) override
    {
        RenderSystem* render = scene().system<RenderSystem>();
        render->environment()->sky = ResourceRef{builtin::kGradientSkyMaterial};
        render->lights().front()->setIntensity(10.0f);
        render->mainCamera()->setFovDegrees(30.0f);
    }
};

struct Fixture {
    Fixture()
    {
        registry.registerComponent<Weapon>().allowMultiple();
        registry.registerComponent<Sword>().allowMultiple();
        registry.registerComponent<Shield>();
        registry.registerComponent<Journal>().allowMultiple();
        registry.registerComponent<Destroyer>();
        registry.registerComponent<Grip>().requiresComponent<Shield>();
        registry.registerComponent<DayNight>();
    }

    // Журнал раньше сцены: сцена при уничтожении останавливает компоненты, и они в него пишут.
    std::vector<std::string> log;
    test::EngineRegistry registry;
    Input input;
    EngineServices services{input, registry};
    Scene scene{services};
};

constexpr float kFrame = 1.0f / 60.0f;

// Ветка в три уровня: World > Body > Arm > Hand, и Door рядом с Body.
struct Branch {
    explicit Branch(Scene& scene)
        : world(scene.createRoot<Node3D>("World"))
        , body(world.addChild<Node3D>("Body"))
        , arm(body.addChild<Node3D>("Arm"))
        , hand(arm.addChild<Node3D>("Hand"))
        , door(world.addChild<Node3D>("Door"))
    {
    }

    Node3D& world;
    Node3D& body;
    Node3D& arm;
    Node3D& hand;
    Node3D& door;
};

// Своё свойство и метод узла в реестре по имени.
const Property& nodeProperty(const TypeRegistry& registry, std::string_view name)
{
    const TypeInfo* node = registry.find("Node");
    REQUIRE(node);
    const auto properties = node->ownProperties();
    const auto it = std::ranges::find(properties, name, &Property::name);
    REQUIRE(it != properties.end());

    return *it;
}

const Method& nodeMethod(const TypeRegistry& registry, std::string_view name)
{
    const TypeInfo* node = registry.find("Node");
    REQUIRE(node);
    const auto methods = node->ownMethods();
    const auto it = std::ranges::find(methods, name, &Method::name);
    REQUIRE(it != methods.end());

    return *it;
}

} // namespace

TEST_CASE_FIXTURE(Fixture, "Node::find: a path of child names, with .. for the parent")
{
    Branch branch(scene);
    CHECK(branch.body.find("Arm/Hand") == &branch.hand);
    CHECK(branch.hand.find("../..") == &branch.body);
    CHECK(branch.hand.find("../../../Door") == &branch.door);
    CHECK(branch.arm.find("") == &branch.arm);
    CHECK(branch.arm.find("./Hand") == &branch.hand);
    CHECK(branch.arm.find("Hand/") == &branch.hand);
    CHECK(branch.body.find("Arm/Leg") == nullptr);
    // Выше корня пути нет.
    CHECK(branch.world.find("..") == nullptr);
    CHECK(branch.world.find("../World") == nullptr);
}

TEST_CASE_FIXTURE(Fixture, "Scene::find: a node by ID or reference, gone once it leaves the scene")
{
    Branch branch(scene);
    CHECK(scene.find(branch.hand.id()) == &branch.hand);
    CHECK(scene.find(NodeRef{branch.door.id()}) == &branch.door);
    CHECK(scene.find(NodeRef{}) == nullptr);

    auto taken = branch.arm.removeChild(branch.hand);
    CHECK(scene.find(taken->id()) == nullptr);
    branch.door.addChild(std::move(taken));
    CHECK(scene.find(branch.door.children().front()->id()) == branch.door.children().front().get());
}

TEST_CASE_FIXTURE(Fixture, "Scene::find: the ID given by the loader is found, the old one is not")
{
    SceneSerializer serializer(registry);
    Branch branch(scene);
    const NodeId hand = branch.hand.id();
    const std::string json = serializer.toJson(scene);

    Scene loaded{services};
    REQUIRE(serializer.fromJson(loaded, json));
    Node* found = loaded.find(hand);
    REQUIRE(found);
    CHECK(found->name() == "Hand");
}

TEST_CASE_FIXTURE(Fixture, "Groups: nodes by group without a walk, after rename, move and destroy")
{
    Branch branch(scene);
    branch.hand.addToGroup("enemies");
    branch.door.addToGroup("enemies");
    branch.door.addToGroup("doors");
    // Повтор и пустое имя ничего не добавляют.
    branch.door.addToGroup("enemies");
    branch.door.addToGroup("");
    CHECK(branch.door.groups() == std::vector<std::string>{"enemies", "doors"});
    // Список из инспектора хранится как задан, индекс берёт только имена.
    branch.door.setGroups({"enemies", "", "doors", "doors"});
    CHECK(branch.door.groups().size() == 4);
    CHECK(scene.group("doors").size() == 1);
    CHECK(scene.group("").empty());
    branch.door.setGroups({"enemies", "doors"});

    const auto enemies = [&] {
        const auto group = scene.group("enemies");

        return std::vector<Node*>(group.begin(), group.end());
    };
    CHECK(enemies() == std::vector<Node*>{&branch.hand, &branch.door});
    CHECK(scene.group("nobody").empty());

    // Имя и место узла группе безразличны.
    branch.hand.setName("Claw");
    scene.setParent(branch.hand, branch.door, KeepWorld::No);
    CHECK(enemies() == std::vector<Node*>{&branch.hand, &branch.door});

    branch.door.removeFromGroup("enemies");
    CHECK(enemies() == std::vector<Node*>{&branch.hand});
    CHECK(branch.door.isInGroup("doors"));
    CHECK_FALSE(branch.door.isInGroup("enemies"));

    // Hand теперь внутри Door: уходит вместе с веткой.
    scene.destroy(branch.door);
    CHECK(scene.group("enemies").empty());
    CHECK(scene.group("doors").empty());
}

TEST_CASE_FIXTURE(Fixture, "Groups: a node outside the scene keeps its groups and joins them on entering")
{
    Branch branch(scene);
    auto pickup = std::make_unique<Node3D>("Coin");
    pickup->setGroups({"pickups", "pickups", "shiny"});
    CHECK(scene.group("pickups").empty());
    Node& coin = branch.world.addChild(std::move(pickup));
    REQUIRE(scene.group("pickups").size() == 1);
    CHECK(scene.group("pickups").front() == &coin);
}

TEST_CASE_FIXTURE(Fixture, "Groups are a property: saved, loaded and set through the registry")
{
    SceneSerializer serializer(registry);
    Branch branch(scene);
    branch.door.setGroups({"doors"});

    Scene loaded{services};
    REQUIRE(serializer.fromJson(loaded, serializer.toJson(scene)));
    REQUIRE(loaded.group("doors").size() == 1);
    CHECK(loaded.group("doors").front()->name() == "Door");

    REQUIRE(nodeProperty(registry, "groups").set(branch.hand, ListValue{{std::string("enemies")}}));
    REQUIRE(scene.group("enemies").size() == 1);
    CHECK(scene.group("enemies").front() == &branch.hand);
}

TEST_CASE_FIXTURE(Fixture, "Components: all of a type, in children and in parents")
{
    Branch branch(scene);
    auto& sword = branch.hand.addComponent<Sword>();
    auto& spare = branch.hand.addComponent<Sword>();
    auto& shield = branch.arm.addComponent<Shield>();

    CHECK(branch.hand.getComponents<Weapon>() == std::vector<Weapon*>{&sword, &spare});
    CHECK(branch.hand.getComponents<Shield>().empty());

    // В детях: сам узел первым, затем в глубину.
    CHECK(branch.body.findComponentInChildren<Weapon>() == &sword);
    CHECK(branch.arm.findComponentInChildren<Shield>() == &shield);
    CHECK(branch.door.findComponentInChildren<Weapon>() == nullptr);
    // Неактивная ветка тоже просматривается.
    branch.arm.setActive(false);
    CHECK(branch.world.findComponentInChildren<Sword>() == &sword);

    // В родителях: от узла к корню.
    CHECK(branch.hand.findComponentInParent<Shield>() == &shield);
    CHECK(branch.arm.findComponentInParent<Shield>() == &shield);
    CHECK(branch.body.findComponentInParent<Shield>() == nullptr);
}

TEST_CASE_FIXTURE(Fixture, "Components by registry name: the type or a derived one")
{
    Branch branch(scene);
    auto& sword = branch.hand.addComponent<Sword>();
    auto& shield = branch.hand.addComponent<Shield>();

    CHECK(componentOfType(branch.hand, "Sword", registry) == &sword);
    // Sword - наследник Weapon в реестре.
    CHECK(componentOfType(branch.hand, "Weapon", registry) == &sword);
    CHECK(componentOfType(branch.hand, "Shield", registry) == &shield);
    CHECK(componentOfType(branch.hand, "Bow", registry) == nullptr);
    CHECK(componentOfType(branch.door, "Sword", registry) == nullptr);
    CHECK(componentsOfType(branch.hand, "Component", registry).size() == 3);
}

TEST_CASE_FIXTURE(Fixture, "Node.find and isInGroup are methods of the registry")
{
    Branch branch(scene);
    branch.hand.addToGroup("enemies");
    const std::vector<PropertyValue> path{std::string("Arm/Hand")};
    const MethodCallResult found = nodeMethod(registry, "find").call(branch.body, path);
    REQUIRE(found.called);
    REQUIRE(found.value);
    CHECK(std::get<NodeRef>(*found.value).id == branch.hand.id());

    const std::vector<PropertyValue> group{std::string("enemies")};
    const MethodCallResult result = nodeMethod(registry, "isInGroup").call(branch.hand, group);
    REQUIRE(result.called);
    REQUIRE(result.value);
    CHECK(std::get<bool>(*result.value));
}

TEST_CASE_FIXTURE(Fixture, "destroy(component) in a frame: at its end, after the others ran")
{
    Branch branch(scene);
    auto& destroyer = branch.hand.addComponent<Destroyer>(&log, "destroyer");
    auto& victim = branch.hand.addComponent<Journal>(&log, "victim");
    destroyer.target = &victim;
    scene.advance(kFrame);

    // Цель доживает кадр: её update и late ещё идут, onDisable и onStop - в конце.
    CHECK(
        log
        == std::vector<std::string>{
            "destroyer update",
            "victim update",
            "destroyer late",
            "victim late",
            "victim disable",
            "victim stop",
        }
    );
    CHECK(branch.hand.getComponents<Journal>() == std::vector<Journal*>{&destroyer});

    log.clear();
    scene.advance(kFrame);
    CHECK(log == std::vector<std::string>{"destroyer update", "destroyer late"});
}

TEST_CASE_FIXTURE(Fixture, "destroy(component): twice, or with its node in one frame, is harmless")
{
    Branch branch(scene);
    auto& first = branch.hand.addComponent<Destroyer>();
    auto& victim = branch.arm.addComponent<Journal>();
    first.target = &victim;
    // Второй Destroyer на соседнем узле уничтожает ту же цель ещё раз.
    auto& second = branch.door.addComponent<Destroyer>();
    second.target = &victim;
    scene.advance(kFrame);
    CHECK(branch.arm.getComponents<Journal>().empty());

    // В одном кадре: компонент узла Arm, затем сам Arm с веткой (Hand с first).
    auto& doomed = branch.arm.addComponent<Journal>();
    first.target = &doomed;
    second.node = &branch.arm;
    const NodeId arm = branch.arm.id();
    const NodeId hand = branch.hand.id();
    scene.advance(kFrame);
    CHECK(scene.find(arm) == nullptr);
    CHECK(scene.find(hand) == nullptr);
    CHECK(scene.find(branch.door.id()) == &branch.door);
}

TEST_CASE_FIXTURE(Fixture, "destroy(component): built-in and required ones stay")
{
    Branch branch(scene);
    auto& grip = branch.hand.addComponent<Grip>();
    Shield* shield = branch.hand.getComponent<Shield>();
    REQUIRE(shield);
    CHECK_FALSE(scene.destroy(*shield));
    CHECK(branch.hand.getComponent<Shield>() == shield);
    CHECK_FALSE(scene.destroy(branch.hand.transform()));
    // Без зависимого щит уже снимается.
    CHECK(scene.destroy(grip));
    CHECK(scene.destroy(*shield));
    CHECK(branch.hand.getComponent<Shield>() == nullptr);
}

TEST_CASE_FIXTURE(Fixture, "Every engine property is set through the registry on a live scene and read back")
{
    auto& root = scene.createRoot<Node3D>("Root");
    std::size_t checked = 0;
    const auto roundTrip = [&](auto& object, const TypeInfo& type) {
        for (const Property* property : registry.properties(type)) {
            CAPTURE(type.name());
            CAPTURE(property->name());
            const PropertyValue value = property->get(object);
            CHECK(property->set(object, value));
            CHECK(property->get(object) == value);
            // Логическое значение меняется и читается обратно.
            if (const bool* flag = std::get_if<bool>(&value)) {
                CHECK(property->set(object, !*flag));
                CHECK(std::get<bool>(property->get(object)) == !*flag);
                property->set(object, *flag);
            }
            ++checked;
        }
    };
    for (const auto& type : registry.types()) {
        if (type->module() != TypeRegistry::engineModule() || !type->creatable()) {
            continue;
        }
        if (type->kind() == TypeKind::Node) {
            Node& node = root.addChild(type->createNode());
            roundTrip(node, *type);
            for (const auto& component : node.components()) {
                if (const TypeInfo* builtin = registry.find(component->typeName()); builtin && builtin->builtin()) {
                    roundTrip(*component, *builtin);
                }
            }
        } else if (type->kind() == TypeKind::Component) {
            auto& holder = root.addChild<Node3D>(std::string(type->name()));
            if (!canAttach(registry, *type, holder)) {
                continue;
            }
            Component& component = holder.addComponent(type->createComponent());
            roundTrip(component, *type);
        }
    }
    CHECK(checked > 50);
}

TEST_CASE_FIXTURE(Fixture, "A script changes the sky, the sun and the camera, and the frame shows it")
{
    auto& root = scene.createRoot<Node3D>("Root");
    root.addChild<Environment>("Environment");
    root.addChild<DirectionalLight3D>("Sun").setIntensity(1000.0f);
    auto& camera = root.addChild<Camera3D>("Camera");
    const FrameData before = extractFrame(scene, 1.0f);
    REQUIRE(before.lights.size() == 1);

    camera.addComponent<DayNight>();
    scene.advance(kFrame);
    const FrameData after = extractFrame(scene, 1.0f);
    REQUIRE(after.environment.sky);
    CHECK(after.materials[*after.environment.sky].id == builtin::kGradientSkyMaterial);
    CHECK(after.lights[0].color.r == doctest::Approx(before.lights[0].color.r / 100.0f));
    // Угол обзора меньше - проекция сильнее увеличивает.
    CHECK(after.views[0].camera.projection[1][1] > before.views[0].camera.projection[1][1]);
}

TEST_CASE_FIXTURE(Fixture, "Properties and methods by name reach the frame like setters")
{
    auto& root = scene.createRoot<Node3D>("Root");
    auto& environment = root.addChild<Environment>("Environment");
    auto& sun = root.addChild<DirectionalLight3D>("Sun");
    auto& camera = root.addChild<Camera3D>("Camera");

    CHECK(setProperty(sun, "intensity", 5.0f, registry));
    CHECK(sun.intensity() == doctest::Approx(5.0f));
    CHECK(std::get<float>(*getProperty(sun, "intensity", registry)) == doctest::Approx(5.0f));
    CHECK(setProperty(camera, "fov", 45.0f, registry));
    CHECK(camera.fovDegrees() == doctest::Approx(45.0f));
    CHECK(setProperty(environment, "sky", ResourceRef{builtin::kGradientSkyMaterial}, registry));
    const FrameData frame = extractFrame(scene, 1.0f);
    REQUIRE(frame.environment.sky);
    CHECK(frame.materials[*frame.environment.sky].id == builtin::kGradientSkyMaterial);

    // Свойство базового типа, неизвестное имя, значение другого типа.
    CHECK(setProperty(sun, "name", std::string("Moon"), registry));
    CHECK(sun.name() == "Moon");
    CHECK_FALSE(setProperty(sun, "brightness", 1.0f, registry));
    CHECK_FALSE(setProperty(sun, "intensity", std::string("bright"), registry));
    CHECK_FALSE(getProperty(sun, "brightness", registry));

    const std::vector<PropertyValue> path{std::string("Moon")};
    const MethodCallResult found = callMethod(root, "findChild", path, registry);
    REQUIRE(found.called);
    CHECK(std::get<NodeRef>(*found.value).id == sun.id());
    CHECK_FALSE(callMethod(root, "explode", {}, registry).called);
}
