#include "EngineRegistry.hpp"

#include <doctest/doctest.h>

#include <yqlora/core/EngineServices.hpp>
#include <yqlora/editor/ComponentSnapshot.hpp>
#include <yqlora/editor/EditorSession.hpp>
#include <yqlora/editor/JointEditing.hpp>
#include <yqlora/editor/Picking.hpp>
#include <yqlora/editor/PointPick.hpp>
#include <yqlora/editor/SceneEditing.hpp>
#include <yqlora/editor/Snapping.hpp>
#include <yqlora/input/Input.hpp>
#include <yqlora/physics/Joint.hpp>
#include <yqlora/physics/RigidBody.hpp>
#include <yqlora/render/MeshRenderer.hpp>
#include <yqlora/resource/BuiltinResources.hpp>
#include <yqlora/scene/Node3D.hpp>
#include <yqlora/scene/Scene.hpp>
#include <yqlora/scene/SceneSerializer.hpp>
#include <yqlora/scene/Transform3D.hpp>
#include <yqlora/scripting/GameModule.hpp>

#include <glm/gtc/quaternion.hpp>

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

    test::EngineRegistry registry;
    SceneSerializer serializer{registry};
    Input input;
    EngineServices services{input, registry};
    GameModule gameModule{registry};
    EditorSession session{services, serializer, gameModule};
    Node3D* root = nullptr;
};

} // namespace

TEST_CASE_FIXTURE(
    Fixture,
    "Joint frame: the anchor and axis go to the world with the node, the normal is the hinge zero"
)
{
    auto& door = root->addChild<Node3D>("Door");
    door.transform().setPosition({2.0f, 0.0f, 0.0f});
    // Четверть оборота вокруг Z: ось Y узла смотрит вдоль -X мира.
    door.transform().setRotation(glm::angleAxis(std::numbers::pi_v<float> * 0.5f, Vec3(0.0f, 0.0f, 1.0f)));
    door.addComponent<RigidBody>();
    auto& hinge = door.addComponent<HingeJoint>();
    hinge.setAnchor({0.0f, 1.0f, 0.0f});
    session.scene().updateTransforms();

    const JointWorldFrame frame = jointWorldFrame(hinge, door);
    CHECK(near(frame.point, {1.0f, 0.0f, 0.0f}));
    CHECK(near(frame.axis, {-1.0f, 0.0f, 0.0f}));
    CHECK(glm::dot(frame.axis, frame.normal) == doctest::Approx(0.0f).epsilon(kTolerance));
    CHECK(near(jointNormal(Vec3(0.0f, 1.0f, 0.0f)), glm::inverse(door.transform().rotation()) * frame.normal));
}

TEST_CASE("Joint frame: the limit arc of a hinge goes from its lower to its upper angle around its axis")
{
    const JointWorldFrame frame{.point = {0.0f, 1.0f, 0.0f}, .axis = {0.0f, 1.0f, 0.0f}, .normal = {1.0f, 0.0f, 0.0f}};
    const auto arc = jointArc(frame, -90.0f, 45.0f, 2.0f, 8);
    REQUIRE(arc.size() == 9);
    // Против часовой, если смотреть с конца оси Y: +90 от X - это -Z.
    CHECK(near(arc.front(), {0.0f, 1.0f, 2.0f}));
    const float half = std::numbers::sqrt2_v<float>;
    CHECK(near(arc.back(), {half, 1.0f, -half}));
    for (const Vec3& point : arc) {
        CHECK(glm::length(point - frame.point) == doctest::Approx(2.0f));
    }
}

TEST_CASE_FIXTURE(Fixture, "Joint frame: a distance joint ends at its connected anchor, others at the other body")
{
    auto& lamp = root->addChild<Node3D>("Lamp");
    lamp.addComponent<RigidBody>();
    auto& ceiling = root->addChild<Node3D>("Ceiling");
    ceiling.transform().setPosition({0.0f, 3.0f, 0.0f});
    session.scene().updateTransforms();

    auto& rope = lamp.addComponent<DistanceJoint>();
    rope.setConnectedAnchor({0.0f, 0.0f, 1.0f});
    CHECK(near(*connectedPoint(rope, session.scene()), {0.0f, 0.0f, 1.0f}));
    rope.setConnectedBody(NodeRef{ceiling.id()});
    CHECK(near(*connectedPoint(rope, session.scene()), {0.0f, 3.0f, 1.0f}));

    auto& point = lamp.addComponent<PointJoint>();
    CHECK_FALSE(connectedPoint(point, session.scene()));
    point.setConnectedBody(NodeRef{ceiling.id()});
    CHECK(near(*connectedPoint(point, session.scene()), {0.0f, 3.0f, 0.0f}));
}

TEST_CASE_FIXTURE(Fixture, "Joint anchor: a drag edited directly is one undo step")
{
    auto& door = root->addChild<Node3D>("Door");
    door.addComponent<RigidBody>();
    auto& hinge = door.addComponent<HingeJoint>();
    const ComponentSnapshot before(registry, hinge);
    hinge.setAnchor({0.1f, 0.0f, 0.0f});
    hinge.setAnchor({0.5f, 1.0f, 0.0f});

    REQUIRE(session.editing().commitComponentEdit(hinge, before));
    CHECK(near(hinge.anchor(), {0.5f, 1.0f, 0.0f}));
    REQUIRE(session.undo());
    CHECK(near(hinge.anchor(), Vec3(0.0f)));
}

TEST_CASE("Ray and plane: the point where a ray meets a plane, nothing behind or along it")
{
    const Ray down{.origin = {1.0f, 5.0f, 2.0f}, .direction = {0.0f, -1.0f, 0.0f}};
    const auto hit = rayPlane(down, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
    REQUIRE(hit);
    CHECK(near(*hit, {1.0f, 1.0f, 2.0f}));
    CHECK_FALSE(rayPlane(down, {0.0f, 6.0f, 0.0f}, {0.0f, 1.0f, 0.0f}));
    CHECK_FALSE(rayPlane(down, {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}));
}

TEST_CASE_FIXTURE(Fixture, "Point pick: a click puts the point on the surface under the ray, on the grid with snapping")
{
    auto& box = root->addChild<Node3D>("Box");
    box.transform().setPosition({0.3f, 0.0f, 0.0f});
    box.addComponent<MeshRenderer>().setMesh(ResourceRef{builtin::kCubeMesh});
    session.scene().updateTransforms();

    Vec3 field{0.0f};
    PointPick& pick = session.pointPick();
    pick.request(&field, session.scene());
    CHECK(pick.waitingFor(&field));
    CHECK_FALSE(pick.take(&field));

    const Ray down{.origin = {0.3f, 10.0f, 0.2f}, .direction = {0.0f, -1.0f, 0.0f}};
    REQUIRE(pick.answer(session.scene(), down, SnapSettings{}));
    CHECK_FALSE(pick.waitingFor(&field));
    const auto point = pick.take(&field);
    REQUIRE(point);
    // Верх куба с центром в 0 - на высоте 0.5.
    CHECK(near(*point, {0.3f, 0.5f, 0.2f}));
    CHECK_FALSE(pick.take(&field));

    pick.request(&field, session.scene());
    REQUIRE(pick.answer(session.scene(), down, SnapSettings{.grid = true, .moveStep = 0.5f}));
    CHECK(near(*pick.take(&field), {0.5f, 0.5f, 0.0f}));
}

TEST_CASE_FIXTURE(Fixture, "Point pick: Escape leaves the field as it was, a request of another scene is dropped")
{
    Vec3 field{1.0f, 2.0f, 3.0f};
    PointPick& pick = session.pointPick();
    pick.request(&field, session.scene());
    pick.cancel();
    CHECK_FALSE(pick.waitingFor(&field));
    CHECK_FALSE(pick.take(&field));

    Scene other(services);
    pick.request(&field, other);
    CHECK_FALSE(pick.waitingIn(session.scene()));
    CHECK_FALSE(pick.waitingFor(&field));
    const Ray down{.origin = {0.0f, 10.0f, 0.0f}, .direction = {0.0f, -1.0f, 0.0f}};
    CHECK_FALSE(pick.answer(session.scene(), down, SnapSettings{}));
    CHECK_FALSE(pick.take(&field));
}

TEST_CASE("Point pick: the guide of a tool is taken once per frame")
{
    PointPick pick;
    pick.showGuide({Vec3(0.0f), Vec3(1.0f)});
    CHECK(pick.takeGuide().size() == 2);
    CHECK(pick.takeGuide().empty());
}
