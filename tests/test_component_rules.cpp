#include <doctest/doctest.h>

#include <yqlora/core/EngineServices.hpp>
#include <yqlora/core/EngineTypes.hpp>
#include <yqlora/editor/EditorSession.hpp>
#include <yqlora/editor/Inspection.hpp>
#include <yqlora/input/Input.hpp>
#include <yqlora/reflection/ComponentRules.hpp>
#include <yqlora/reflection/TypeRegistry.hpp>
#include <yqlora/render/Camera3D.hpp>
#include <yqlora/render/MeshRenderer.hpp>
#include <yqlora/scene/MissingComponent.hpp>
#include <yqlora/scene/Scene.hpp>
#include <yqlora/scene/SceneSerializer.hpp>
#include <yqlora/scripting/GameModule.hpp>

#include <algorithm>
#include <memory>

using namespace yqlora;
using namespace yqlora::editor;

namespace {

struct Body : Component {
    YQLORA_TYPE(Body, Component)
};

// Тоже тело: закрывает требование Body.
struct HeavyBody final : Body {
    YQLORA_TYPE(HeavyBody, Body)
};

struct TestCollider : Component {
    YQLORA_TYPE(TestCollider, Component)
};

// Требования базового TestCollider действуют и на него.
struct TestBoxCollider final : TestCollider {
    YQLORA_TYPE(TestBoxCollider, TestCollider)
};

struct Sound final : Component {
    YQLORA_TYPE(Sound, Component)
};

// Цепочка и цикл требований.
struct First final : Component {
    YQLORA_TYPE(First, Component)
};

struct Second final : Component {
    YQLORA_TYPE(Second, Component)
};

struct Third final : Component {
    YQLORA_TYPE(Third, Component)
};

struct Unregistered final : Component {
    YQLORA_TYPE(Unregistered, Component)
};

struct NeedsUnregistered final : Component {
    YQLORA_TYPE(NeedsUnregistered, Component)
};

struct Fixture {
    Fixture()
    {
        registerEngineTypes(registry);
        registry.registerComponent<Body>().requiresNode<Node3D>();
        registry.registerComponent<HeavyBody>();
        registry.registerComponent<TestCollider>().requiresComponent<Body>();
        registry.registerComponent<TestBoxCollider>();
        registry.registerComponent<Sound>().allowMultiple();
        registry.registerComponent<First>().requiresComponent<Second>();
        registry.registerComponent<Second>().requiresComponent<Third>();
        registry.registerComponent<Third>().requiresComponent<Second>();
        registry.registerComponent<NeedsUnregistered>().requiresComponent<Unregistered>();
    }

    const TypeInfo& type(std::string_view name) const
    {
        return *registry.find(name);
    }

    std::vector<std::string> names(const std::vector<const TypeInfo*>& types) const
    {
        std::vector<std::string> result;
        for (const TypeInfo* t : types) {
            result.push_back(t->name());
        }

        return result;
    }

    TypeRegistry registry;
};

} // namespace

TEST_CASE_FIXTURE(Fixture, "ComponentRules: requiresNode accepts the type and its descendants only")
{
    const Node plain;
    const Node3D node3D;
    const Camera3D camera;

    CHECK_FALSE(canAttach(registry, type("MeshRenderer"), plain));
    CHECK(canAttach(registry, type("MeshRenderer"), node3D));
    CHECK(canAttach(registry, type("MeshRenderer"), camera));

    // Правило базового Body действует и на HeavyBody.
    CHECK_FALSE(canAttach(registry, type("HeavyBody"), plain));
    CHECK(canAttach(registry, type("HeavyBody"), node3D));
}

TEST_CASE_FIXTURE(Fixture, "ComponentRules: one component of a type unless allowMultiple")
{
    Node3D node;
    node.addComponent<MeshRenderer>();
    node.addComponent<Sound>();

    CHECK_FALSE(canAttach(registry, type("MeshRenderer"), node));
    CHECK(canAttach(registry, type("Sound"), node));
}

TEST_CASE_FIXTURE(Fixture, "ComponentRules: missing requirements come in the order to add them")
{
    Node3D node;
    REQUIRE(missingRequirements(registry, type("TestCollider"), node));
    CHECK(names(*missingRequirements(registry, type("TestCollider"), node)) == std::vector<std::string>{"Body"});

    // Требование унаследовано от TestCollider.
    CHECK(names(*missingRequirements(registry, type("TestBoxCollider"), node)) == std::vector<std::string>{"Body"});

    // Наследник требуемого типа тоже подходит.
    node.addComponent<HeavyBody>();
    CHECK(missingRequirements(registry, type("TestCollider"), node)->empty());
}

TEST_CASE_FIXTURE(Fixture, "ComponentRules: requirements of requirements, and cycles end")
{
    const Node3D node;
    // First -> Second -> Third -> Second: Second уже решено добавить, цикл обрывается.
    CHECK(names(*missingRequirements(registry, type("First"), node)) == std::vector<std::string>{"Third", "Second"});
}

TEST_CASE_FIXTURE(Fixture, "ComponentRules: a requirement that cannot be met")
{
    const Node plain;
    const Node3D node;
    // Тип требования не зарегистрирован.
    CHECK_FALSE(missingRequirements(registry, type("NeedsUnregistered"), node));
    // Body не живёт на голом Node, значит и TestCollider на нём не собрать.
    CHECK_FALSE(missingRequirements(registry, type("TestCollider"), plain));
}

TEST_CASE_FIXTURE(Fixture, "ComponentRules: dependents of a component")
{
    Node3D node;
    auto& body = node.addComponent<Body>();
    auto& collider = node.addComponent<TestBoxCollider>();

    CHECK(dependentsOf(registry, node, body) == std::vector<const Component*>{&collider});
    CHECK(dependentsOf(registry, node, collider).empty());

    // Есть другое тело: без первого коллайдер не останется.
    node.addComponent<HeavyBody>();
    CHECK(dependentsOf(registry, node, body).empty());
}

TEST_CASE_FIXTURE(Fixture, "EditorSession: component rules")
{
    SceneSerializer serializer{registry};
    Input input;
    EngineServices services{input, registry};
    GameModule gameModule{registry};
    EditorSession session{services, serializer, gameModule};
    Node* node = session.editing().createNode(type("Node3D"), nullptr);
    REQUIRE(node);

    const auto offers = [&](std::string_view name) {
        return std::ranges::any_of(attachableTypes(registry, *node), [&](const TypeInfo* t) {
            return t->name() == name;
        });
    };
    CHECK(offers("MeshRenderer"));
    CHECK_FALSE(offers("NeedsUnregistered"));
    // Встроенный Transform3D не создаётся.
    CHECK_FALSE(offers("Transform3D"));

    SUBCASE("a required component is added in the same history step")
    {
        Component* collider = session.editing().addComponent(*node, type("TestBoxCollider"));
        REQUIRE(collider);
        REQUIRE(node->components().size() == 3);
        CHECK(node->components()[1]->typeName() == "Body");
        CHECK(node->components()[2].get() == collider);

        // Тело нужно коллайдеру: снять нельзя.
        Component& body = *node->components()[1];
        CHECK(dependentsOf(registry, *node, body) == std::vector<const Component*>{collider});
        CHECK_FALSE(session.editing().removeComponent(*node, body));
        CHECK(node->components().size() == 3);

        REQUIRE(session.undo());
        CHECK(node->components().size() == 1);
        REQUIRE(session.redo());
        CHECK(node->components().size() == 3);
    }

    SUBCASE("no second component of a type and none that does not fit")
    {
        REQUIRE(session.editing().addComponent(*node, type("MeshRenderer")));
        CHECK_FALSE(offers("MeshRenderer"));
        CHECK_FALSE(session.editing().addComponent(*node, type("MeshRenderer")));

        Node* plain = session.editing().createNode(type("Node"), node);
        REQUIRE(plain);
        CHECK_FALSE(session.editing().addComponent(*plain, type("MeshRenderer")));
        CHECK(plain->components().empty());
    }
}

TEST_CASE_FIXTURE(Fixture, "SceneSerializer: component rules on load")
{
    const SceneSerializer serializer{registry};

    Scene source{EngineServices::none()};
    auto& root = source.createRoot<Node3D>("Root");
    root.addComponent<TestBoxCollider>();                      // тела нет: файл как будто правили руками
    root.addChild<Node>("Plain").addComponent<MeshRenderer>(); // не тот узел

    Scene loaded{EngineServices::none()};
    REQUIRE(serializer.fromJson(loaded, serializer.toJson(source)));
    const Node* loadedRoot = loaded.root();
    REQUIRE(loadedRoot);

    // Обязательное тело добавлено.
    REQUIRE(loadedRoot->components().size() == 3);
    CHECK(loadedRoot->components()[1]->typeName() == "TestBoxCollider");
    CHECK(loadedRoot->components()[2]->typeName() == "Body");

    // Компонент на неподходящем узле пропущен.
    REQUIRE(loadedRoot->children().size() == 1);
    CHECK(loadedRoot->children()[0]->components().empty());
}

TEST_CASE_FIXTURE(Fixture, "Inspection: what the inspector shows for the selected node")
{
    SceneSerializer serializer{registry};
    Input input;
    EngineServices services{input, registry};
    GameModule gameModule{registry};
    EditorSession session{services, serializer, gameModule};
    CHECK_FALSE(inspect(session));

    Node* node = session.editing().createNode(type("Node3D"), nullptr);
    REQUIRE(node);
    REQUIRE(session.editing().addComponent(*node, type("TestBoxCollider")));

    const auto inspection = inspect(session);
    REQUIRE(inspection);
    CHECK(&inspection->node == node);
    CHECK(inspection->type.name() == "Node3D");
    REQUIRE(inspection->components.size() == 3);

    // Transform3D встроенный, Body нужен коллайдеру, коллайдер снять можно.
    const auto& transform = inspection->components[0];
    const auto& body = inspection->components[1];
    const auto& collider = inspection->components[2];
    CHECK_FALSE(transform.removable);
    CHECK_FALSE(body.removable);
    CHECK(body.requiredBy == std::vector<const Component*>{&collider.component});
    CHECK(collider.removable);
    CHECK(collider.type == &type("TestBoxCollider"));

    // Уже есть Body, второго не предложат.
    CHECK(std::ranges::none_of(inspection->addable, [&](const TypeInfo* t) {
        return t->name() == "Body";
    }));

    // Заглушка компонента пропавшего типа видна под его именем, и её можно снять.
    node->addComponent(std::make_unique<MissingComponent>(nlohmann::json{{"type", "Gone"}}));
    const auto withMissing = inspect(session);
    REQUIRE(withMissing);
    const auto& missing = withMissing->components.back();
    CHECK(missing.typeName == "Gone");
    CHECK(missing.type == nullptr);
    CHECK(missing.removable);
    CHECK(transform.typeName == "Transform3D");
}

TEST_CASE_FIXTURE(Fixture, "Node: addComponent in a scene adds the required components first")
{
    const Input input;
    const EngineServices services(input, registry);
    Scene scene{services};
    auto& root = scene.createRoot<Node3D>();

    auto& collider = root.addComponent<TestBoxCollider>();
    REQUIRE(root.components().size() == 3);
    CHECK(root.components()[1]->typeName() == "Body");
    CHECK(root.components()[2].get() == &collider);

    // Вне сцены правила не проверяются: узел ещё собирают.
    Node3D detached;
    detached.addComponent<TestBoxCollider>();
    CHECK(detached.components().size() == 2);
}
