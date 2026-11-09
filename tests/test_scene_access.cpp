#include "EngineRegistry.hpp"

#include <doctest/doctest.h>

#include <yqlora/core/EngineServices.hpp>
#include <yqlora/input/Input.hpp>
#include <yqlora/reflection/ComponentLookup.hpp>
#include <yqlora/reflection/Method.hpp>
#include <yqlora/reflection/Property.hpp>
#include <yqlora/scene/Node3D.hpp>
#include <yqlora/scene/Scene.hpp>
#include <yqlora/scene/SceneSerializer.hpp>

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
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

struct Fixture {
    Fixture()
    {
        registry.registerComponent<Weapon>().allowMultiple();
        registry.registerComponent<Sword>().allowMultiple();
        registry.registerComponent<Shield>();
    }

    test::EngineRegistry registry;
    Input input;
    EngineServices services{input, registry};
    Scene scene{services};
};

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
