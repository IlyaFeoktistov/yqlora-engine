#include "EngineRegistry.hpp"

#include <doctest/doctest.h>

#include <yqlora/core/EngineServices.hpp>
#include <yqlora/editor/ColliderEditing.hpp>
#include <yqlora/editor/ComponentSnapshot.hpp>
#include <yqlora/editor/EditorSession.hpp>
#include <yqlora/editor/ReferenceFields.hpp>
#include <yqlora/editor/ResourceChoices.hpp>
#include <yqlora/editor/SceneEditing.hpp>
#include <yqlora/editor/ShapeHandles.hpp>
#include <yqlora/input/Input.hpp>
#include <yqlora/physics/BoxCollider.hpp>
#include <yqlora/physics/CapsuleCollider.hpp>
#include <yqlora/physics/Joint.hpp>
#include <yqlora/physics/MeshCollider.hpp>
#include <yqlora/physics/PhysicsMaterialData.hpp>
#include <yqlora/physics/RigidBody.hpp>
#include <yqlora/physics/SphereCollider.hpp>
#include <yqlora/reflection/TypeRegistry.hpp>
#include <yqlora/render/MeshRenderer.hpp>
#include <yqlora/resource/BuiltinResources.hpp>
#include <yqlora/scene/Node3D.hpp>
#include <yqlora/scene/SceneSerializer.hpp>
#include <yqlora/scene/Transform3D.hpp>
#include <yqlora/scripting/GameModule.hpp>

#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>

using namespace yqlora;
using namespace yqlora::editor;

namespace {

constexpr float kTolerance = 1e-4f;

bool near(const Vec3& a, const Vec3& b)
{
    return glm::length(a - b) < kTolerance;
}

struct Fixture {
    Fixture()
    {
        root = static_cast<Node3D*>(session.editing().createNode(*registry.find("Node3D"), nullptr));
    }

    // Ребёнок parent с мешем-кубом размера size со сдвигом offset.
    static Node3D& cubeChild(Node& parent, const char* name, const Vec3& size, const Vec3& offset)
    {
        auto& node = parent.addChild<Node3D>(name);
        node.transform().setPosition(offset);
        node.transform().setScale(size);
        node.addComponent<MeshRenderer>().setMesh(ResourceRef{builtin::kCubeMesh});

        return node;
    }

    template <typename T>
    T* add(Node& node)
    {
        return dynamic_cast<T*>(session.editing().addComponent(node, *registry.find<T>()));
    }

    test::EngineRegistry registry;
    SceneSerializer serializer{registry};
    Input input;
    EngineServices services{input, registry};
    GameModule gameModule{registry};
    EditorSession session{services, serializer, gameModule};
    Node3D* root = nullptr;
};

} // namespace

TEST_CASE("Shape handles: a box face moves alone, with symmetric both faces move and the centre stays")
{
    const HandleShape box{.center = {1.0f, 0.0f, 0.0f}, .halfExtents = {0.5f, 1.0f, 1.5f}};
    const ShapeHandle top{.axis = 1, .side = 1.0f};

    const HandleShape grown = dragShapeHandle(box, top, 0.5f, {});
    CHECK(grown.halfExtents.y == doctest::Approx(1.25f));
    CHECK(near(grown.center, {1.0f, 0.25f, 0.0f}));
    CHECK(near(handlePosition(grown, {.axis = 1, .side = -1.0f}), handlePosition(box, {.axis = 1, .side = -1.0f})));

    const HandleShape symmetric = dragShapeHandle(box, top, 0.5f, {.symmetric = true});
    CHECK(symmetric.halfExtents.y == doctest::Approx(1.5f));
    CHECK(near(symmetric.center, box.center));
    CHECK(symmetric.halfExtents.x == doctest::Approx(0.5f));
}

TEST_CASE("Shape handles: snapping keeps the size on the step, the minimum is never crossed")
{
    const HandleShape box{.halfExtents = {0.5f, 0.5f, 0.5f}};
    const ShapeHandle right{.axis = 0, .side = 1.0f};

    // Размер 1 + 0.3 = 1.3 ближе всего к 1.25 при шаге 0.25.
    const HandleShape snapped = dragShapeHandle(box, right, 0.3f, {.snapStep = 0.25f});
    CHECK(snapped.halfExtents.x == doctest::Approx(0.625f));

    const HandleShape tiny = dragShapeHandle(box, right, -5.0f, {.minHalfExtent = 0.01f});
    CHECK(tiny.halfExtents.x == doctest::Approx(0.01f));
    // Левая грань осталась на месте.
    CHECK(tiny.center.x - tiny.halfExtents.x == doctest::Approx(-0.5f));
}

TEST_CASE("Shape handles: a sphere grows on every axis, a capsule radius keeps X and Z together")
{
    const HandleShape sphere{.halfExtents = Vec3(0.5f), .links = ShapeLinks::All};
    const HandleShape bigger = dragShapeHandle(sphere, {.axis = 2, .side = -1.0f}, 1.0f, {});
    CHECK(near(bigger.halfExtents, Vec3(1.0f)));
    CHECK(near(bigger.center, {0.0f, 0.0f, -0.5f}));

    const HandleShape capsule{.halfExtents = {0.5f, 1.0f, 0.5f}, .links = ShapeLinks::Radial};
    const HandleShape wider = dragShapeHandle(capsule, {.axis = 0, .side = 1.0f}, 0.4f, {.symmetric = true});
    CHECK(near(wider.halfExtents, {0.9f, 1.0f, 0.9f}));
    const HandleShape taller = dragShapeHandle(capsule, {.axis = 1, .side = 1.0f}, 1.0f, {.symmetric = true});
    CHECK(near(taller.halfExtents, {0.5f, 2.0f, 0.5f}));
}

TEST_CASE("Shape handles: a rotated shape moves its handles and its centre along its own axes")
{
    const Quat quarterAroundZ = glm::angleAxis(std::numbers::pi_v<float> * 0.5f, Vec3(0.0f, 0.0f, 1.0f));
    const HandleShape box{.rotation = quarterAroundZ, .halfExtents = {1.0f, 0.5f, 0.5f}};
    const ShapeHandle plusX{.axis = 0, .side = 1.0f};

    // Ось X фигуры после поворота смотрит вдоль Y узла.
    CHECK(near(handleDirection(box, plusX), {0.0f, 1.0f, 0.0f}));
    CHECK(near(handlePosition(box, plusX), {0.0f, 1.0f, 0.0f}));
    const HandleShape grown = dragShapeHandle(box, plusX, 2.0f, {});
    CHECK(near(grown.center, {0.0f, 1.0f, 0.0f}));
}

TEST_CASE("Shape handles: the point on the handle line nearest to a ray")
{
    const Ray ray{.origin = {2.0f, 5.0f, 0.0f}, .direction = {0.0f, -1.0f, 0.0f}};
    const auto t = closestOnLine(ray, {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f});
    REQUIRE(t);
    CHECK(*t == doctest::Approx(2.0f));
    CHECK_FALSE(closestOnLine(ray, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}));
}

TEST_CASE_FIXTURE(Fixture, "Collider handles: a box, a sphere and a capsule turn into handle shapes and back")
{
    auto& node = root->addChild<Node3D>("Body");
    auto& box = node.addComponent<BoxCollider>();
    box.setSize({1.0f, 2.0f, 3.0f});
    box.setCenter({0.0f, 1.0f, 0.0f});
    const auto shape = colliderHandleShape(box);
    REQUIRE(shape);
    CHECK(near(shape->halfExtents, {0.5f, 1.0f, 1.5f}));

    applyHandleShape(box, dragShapeHandle(*shape, {.axis = 2, .side = 1.0f}, 1.0f, {}));
    CHECK(near(box.size(), {1.0f, 2.0f, 4.0f}));
    CHECK(near(box.center(), {0.0f, 1.0f, 0.5f}));

    auto& capsule = node.addComponent<CapsuleCollider>();
    const auto capsuleShape = colliderHandleShape(capsule);
    REQUIRE(capsuleShape);
    CHECK(capsuleShape->links == ShapeLinks::Radial);
    applyHandleShape(capsule, dragShapeHandle(*capsuleShape, {.axis = 1, .side = 1.0f}, 1.0f, {.symmetric = true}));
    CHECK(capsule.height() == doctest::Approx(4.0f));

    CHECK_FALSE(colliderHandleShape(node.addComponent<MeshCollider>()));
}

TEST_CASE_FIXTURE(Fixture, "Collider handles: a drag edited directly is one undo step that brings back size and centre")
{
    auto& node = root->addChild<Node3D>("Body");
    auto& box = node.addComponent<BoxCollider>();
    const ComponentSnapshot before(registry, box);

    const auto shape = colliderHandleShape(box);
    REQUIRE(shape);
    // Пока тянут, компонент меняется напрямую много раз.
    for (const float distance : {0.1f, 0.3f, 0.5f}) {
        applyHandleShape(box, dragShapeHandle(*shape, {.axis = 0, .side = 1.0f}, distance, {}));
    }
    REQUIRE(session.editing().commitComponentEdit(box, before));
    CHECK(near(box.size(), {1.5f, 1.0f, 1.0f}));
    CHECK(near(box.center(), {0.25f, 0.0f, 0.0f}));

    REQUIRE(session.undo());
    CHECK(near(box.size(), Vec3(1.0f)));
    CHECK(near(box.center(), Vec3(0.0f)));

    REQUIRE(session.redo());
    CHECK(near(box.size(), {1.5f, 1.0f, 1.0f}));

    const ComponentSnapshot unchanged(registry, box);
    CHECK_FALSE(session.editing().commitComponentEdit(box, unchanged));
}

TEST_CASE_FIXTURE(
    Fixture,
    "Fitting: a new box takes the size and centre of the meshes, a capsule its height and radius"
)
{
    auto& node = root->addChild<Node3D>("Crate");
    cubeChild(node, "Mesh", {2.0f, 1.0f, 3.0f}, {0.0f, 0.5f, 0.0f});

    auto* box = add<BoxCollider>(node);
    REQUIRE(box);
    CHECK(near(box->size(), {2.0f, 1.0f, 3.0f}));
    CHECK(near(box->center(), {0.0f, 0.5f, 0.0f}));

    auto* capsule = add<CapsuleCollider>(node);
    REQUIRE(capsule);
    CHECK(capsule->height() == doctest::Approx(1.0f));
    CHECK(capsule->radius() == doctest::Approx(1.5f));

    auto* sphere = add<SphereCollider>(node);
    REQUIRE(sphere);
    CHECK(sphere->radius() == doctest::Approx(1.5f));

    // Добавление с подгонкой - один шаг отмены, и повтор подгоняет снова.
    REQUIRE(session.undo());
    CHECK(node.getComponent<SphereCollider>() == nullptr);
    REQUIRE(session.redo());
    REQUIRE(node.getComponent<SphereCollider>());
    CHECK(node.getComponent<SphereCollider>()->radius() == doctest::Approx(1.5f));
}

TEST_CASE_FIXTURE(Fixture, "Fitting: meshes of a child body are not counted, a node without meshes keeps the defaults")
{
    auto& node = root->addChild<Node3D>("Cart");
    cubeChild(node, "Base", Vec3(1.0f), {0.0f, 0.0f, 0.0f});
    cubeChild(node, "Wheel", Vec3(1.0f), {5.0f, 0.0f, 0.0f}).addComponent<RigidBody>();

    auto* box = add<BoxCollider>(node);
    REQUIRE(box);
    CHECK(near(box->size(), Vec3(1.0f)));
    CHECK(near(box->center(), Vec3(0.0f)));

    auto& empty = root->addChild<Node3D>("Empty");
    auto* plain = add<BoxCollider>(empty);
    REQUIRE(plain);
    CHECK(near(plain->size(), Vec3(1.0f)));
    CHECK_FALSE(canFitCollider(*plain, empty, session.resources()));
    CHECK_FALSE(session.editing().fitCollider(*plain));
}

TEST_CASE_FIXTURE(Fixture, "Fitting: a mesh collider takes the renderer's mesh; Fit to mesh is one undo step")
{
    Node3D& node = cubeChild(*root, "Rock", Vec3(1.0f), Vec3(0.0f));
    auto* mesh = add<MeshCollider>(node);
    REQUIRE(mesh);
    CHECK(mesh->mesh().id == builtin::kCubeMesh);

    auto& box = node.addComponent<BoxCollider>();
    box.setSize({5.0f, 5.0f, 5.0f});
    box.setRotationDegrees({0.0f, 45.0f, 0.0f});
    REQUIRE(session.editing().fitCollider(box));
    CHECK(near(box.size(), Vec3(1.0f)));
    CHECK(near(box.rotationDegrees(), Vec3(0.0f)));

    REQUIRE(session.undo());
    CHECK(near(box.size(), Vec3(5.0f)));
    CHECK(near(box.rotationDegrees(), {0.0f, 45.0f, 0.0f}));
}

TEST_CASE_FIXTURE(Fixture, "Body colliders: from the node with the RigidBody down, without child bodies")
{
    auto& body = root->addChild<Node3D>("Body");
    body.addComponent<RigidBody>();
    auto& own = body.addComponent<BoxCollider>();
    auto& part = body.addChild<Node3D>("Part");
    auto& partCollider = part.addComponent<SphereCollider>();
    auto& other = body.addChild<Node3D>("Other");
    other.addComponent<RigidBody>();
    other.addComponent<BoxCollider>();

    const auto colliders = bodyColliders(part);
    CHECK(colliders.size() == 2);
    CHECK(std::ranges::find(colliders, &own) != colliders.end());
    CHECK(std::ranges::find(colliders, &partCollider) != colliders.end());
}

TEST_CASE_FIXTURE(Fixture, "Drops onto physics fields: a physics material fits a collider, a body node fits a joint")
{
    auto& node = root->addChild<Node3D>("Door");
    node.addComponent<RigidBody>();
    auto& frame = root->addChild<Node3D>("Frame");
    frame.addComponent<RigidBody>();

    const auto properties = registry.properties(*registry.find<BoxCollider>());
    const auto material = std::ranges::find(properties, "material", &Property::name);
    REQUIRE(material != properties.end());
    CHECK((*material)->attributes().resourceKind == PhysicsMaterialData::kResourceKind);
    CHECK(
        resourceForField(
            (*material)->attributes().resourceKind,
            (*material)->attributes().resourceUse,
            builtin::kDefaultPhysicsMaterial,
            nullptr,
            session.resources()
        )
        == builtin::kDefaultPhysicsMaterial
    );
    CHECK_FALSE(resourceForField(
        (*material)->attributes().resourceKind,
        (*material)->attributes().resourceUse,
        builtin::kCubeMesh,
        nullptr,
        session.resources()
    ));

    const auto jointProperties = registry.properties(*registry.find<HingeJoint>());
    const auto connected = std::ranges::find(jointProperties, "connectedBody", &Property::name);
    REQUIRE(connected != jointProperties.end());
    CHECK(nodeFits(**connected, frame, registry));
}
