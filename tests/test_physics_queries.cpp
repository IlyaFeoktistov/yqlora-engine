#include "EngineRegistry.hpp"

#include <doctest/doctest.h>

#include <yqlora/core/EngineServices.hpp>
#include <yqlora/input/Input.hpp>
#include <yqlora/physics/BoxCollider.hpp>
#include <yqlora/physics/PhysicsWorld.hpp>
#include <yqlora/physics/RigidBody.hpp>
#include <yqlora/physics/SphereCollider.hpp>
#include <yqlora/scene/Node3D.hpp>
#include <yqlora/scene/Scene.hpp>
#include <yqlora/scene/Transform3D.hpp>

#include <glm/gtc/quaternion.hpp>

#include <string>

using namespace yqlora;

namespace {

constexpr float kFrame = 1.0f / 60.0f;
constexpr int kFramesPerSecond = 60;
constexpr int kHiddenLayer = 5;
const Vec3 kForward{1.0f, 0.0f, 0.0f};
const Quat kNoRotation{1.0f, 0.0f, 0.0f, 0.0f};

struct Fixture {
    explicit Fixture(bool playing = true)
    {
        scene.setPlaying(playing);
        scene.createRoot<Node3D>("Root");
    }

    // Ящик 1x1x1 без RigidBody: неподвижное тело.
    Node3D& box(const std::string& name, Vec3 position)
    {
        auto& node = scene.root()->addChild<Node3D>(name);
        node.transform().setPosition(position);
        node.addComponent<BoxCollider>();

        return node;
    }

    // Динамическое тело без гравитации и затухания.
    RigidBody& floating(const std::string& name, Vec3 position, float mass = 1.0f)
    {
        auto& node = scene.root()->addChild<Node3D>(name);
        node.transform().setPosition(position);
        auto& body = node.addComponent<RigidBody>();
        body.setInterpolation(false);
        body.setGravityScale(0.0f);
        body.setLinearDamping(0.0f);
        body.setAngularDamping(0.0f);
        body.setMass(mass);
        node.addComponent<SphereCollider>();

        return body;
    }

    void run(float seconds)
    {
        for (int i = 0; i < static_cast<int>(seconds * kFramesPerSecond); ++i) {
            scene.advance(kFrame);
        }
    }

    PhysicsWorld& physics()
    {
        return *scene.system<PhysicsWorld>();
    }

    test::EngineRegistry registry;
    Input input;
    EngineServices services{input, registry};
    Scene scene{services};
};

} // namespace

TEST_CASE_FIXTURE(Fixture, "Queries: a ray hits the nearest of three boxes and gives its normal")
{
    box("Far", {9.0f, 0.0f, 0.0f});
    Node3D& near = box("Near", {3.0f, 0.0f, 0.0f});
    box("Middle", {6.0f, 0.0f, 0.0f});
    const auto hit = physics().raycast(Vec3(0.0f), kForward * 3.0f, 20.0f);
    REQUIRE(hit);
    CHECK(hit->node.id == near.id());
    CHECK(hit->collider.component == near.getComponent<BoxCollider>()->id());
    CHECK(hit->distance == doctest::Approx(2.5f).epsilon(0.01));
    CHECK(hit->point.x == doctest::Approx(2.5f).epsilon(0.01));
    CHECK(glm::dot(hit->normal, -kForward) > 0.99f);

    const auto all = physics().raycastAll(Vec3(0.0f), kForward, 20.0f);
    REQUIRE(all.size() == 3);
    CHECK(all[0].distance < all[1].distance);
    CHECK(all[1].distance < all[2].distance);
    CHECK_FALSE(physics().raycast(Vec3(0.0f), kForward, 2.0f));
}

TEST_CASE_FIXTURE(Fixture, "Queries: the mask skips a layer, triggers are seen only when asked")
{
    Node3D& hidden = box("Hidden", {3.0f, 0.0f, 0.0f});
    hidden.setLayer(kHiddenLayer);
    Node3D& zone = box("Zone", {5.0f, 0.0f, 0.0f});
    zone.getComponent<BoxCollider>()->setTrigger(true);
    Node3D& wall = box("Wall", {7.0f, 0.0f, 0.0f});

    const QueryFilter withoutHidden{.mask = kAllLayers & ~(LayerMask{1} << kHiddenLayer)};
    const auto pastHidden = physics().raycast(Vec3(0.0f), kForward, 20.0f, withoutHidden);
    REQUIRE(pastHidden);
    CHECK(pastHidden->node.id == wall.id());
    const auto first = physics().raycast(Vec3(0.0f), kForward, 20.0f);
    REQUIRE(first);
    CHECK(first->node.id == hidden.id());

    const QueryFilter triggers{.mask = withoutHidden.mask, .includeTriggers = true};
    const auto inZone = physics().raycast(Vec3(0.0f), kForward, 20.0f, triggers);
    REQUIRE(inZone);
    CHECK(inZone->node.id == zone.id());
}

TEST_CASE_FIXTURE(Fixture, "Queries: shapes are cast and overlapped like colliders")
{
    Node3D& wall = box("Wall", {5.0f, 0.0f, 0.0f});
    Node3D& other = box("Other", {0.0f, 5.0f, 0.0f});
    const auto sphere = physics().sphereCast(Vec3(0.0f), 0.5f, kForward, 20.0f);
    REQUIRE(sphere);
    CHECK(sphere->node.id == wall.id());
    CHECK(sphere->distance == doctest::Approx(4.0f).epsilon(0.02));
    CHECK(glm::dot(sphere->normal, -kForward) > 0.99f);
    const auto boxHit = physics().boxCast(Vec3(0.0f), Vec3(0.5f), kNoRotation, kForward, 20.0f);
    REQUIRE(boxHit);
    CHECK(boxHit->distance == doctest::Approx(4.0f).epsilon(0.02));
    const auto capsule = physics().capsuleCast(Vec3(0.0f), 0.25f, 1.0f, kNoRotation, kForward, 20.0f);
    REQUIRE(capsule);
    CHECK(capsule->distance == doctest::Approx(4.25f).epsilon(0.02));

    const auto touching = physics().overlapSphere({2.5f, 2.5f, 0.0f}, 3.0f);
    REQUIRE(touching.size() == 2);
    CHECK(physics().overlapBox({5.0f, 0.0f, 0.0f}, Vec3(0.1f), kNoRotation).size() == 1);
    CHECK(physics().overlapCapsule({0.0f, 5.0f, 0.0f}, 0.1f, 0.5f, kNoRotation).front().node == other.id());
    CHECK(physics().overlapSphere({-10.0f, 0.0f, 0.0f}, 1.0f).empty());
}

TEST_CASE("Queries: an edited scene that does not step sees nodes where the editor put them")
{
    Fixture fixture(false);
    Node3D& wall = fixture.box("Wall", {3.0f, 0.0f, 0.0f});
    REQUIRE(fixture.physics().raycast(Vec3(0.0f), kForward, 20.0f));
    wall.transform().setPosition({0.0f, 10.0f, 0.0f});
    CHECK_FALSE(fixture.physics().raycast(Vec3(0.0f), kForward, 20.0f));
    CHECK(fixture.physics().raycast(Vec3(0.0f), {0.0f, 1.0f, 0.0f}, 20.0f));
    CHECK(fixture.physics().stepCount() == 0);
}

TEST_CASE_FIXTURE(Fixture, "RigidBody: impulses and forces give the velocity their mode says")
{
    RigidBody& heavy = floating("Heavy", {0.0f, 0.0f, 0.0f}, 2.0f);
    heavy.addForce({4.0f, 0.0f, 0.0f}, ForceMode::Impulse);
    CHECK(heavy.velocity().x == doctest::Approx(2.0f));
    heavy.setVelocity(Vec3(0.0f));
    heavy.addForce({3.0f, 0.0f, 0.0f}, ForceMode::VelocityChange);
    CHECK(heavy.velocity().x == doctest::Approx(3.0f));

    // Сила действует один шаг: v = F / m * шаг.
    RigidBody& pushed = floating("Pushed", {0.0f, 10.0f, 0.0f}, 2.0f);
    pushed.addForce({0.0f, 0.0f, 10.0f});
    scene.advance(kFrame);
    CHECK(pushed.velocity().z == doctest::Approx(10.0f / 2.0f * kFrame).epsilon(0.01));
    scene.advance(kFrame);
    CHECK(pushed.velocity().z == doctest::Approx(10.0f / 2.0f * kFrame).epsilon(0.01));

    RigidBody& spun = floating("Spun", {0.0f, 20.0f, 0.0f});
    spun.addTorque({0.0f, 2.0f, 0.0f}, ForceMode::VelocityChange);
    CHECK(spun.angularVelocity().y == doctest::Approx(2.0f).epsilon(0.01));
}

TEST_CASE_FIXTURE(Fixture, "RigidBody: a sleeping body wakes when hit, sleep and wakeUp from code")
{
    RigidBody& sleeper = floating("Sleeper", {0.0f, 0.0f, 0.0f});
    sleeper.sleep();
    CHECK(sleeper.isSleeping());
    run(0.5f);
    CHECK(sleeper.isSleeping());
    RigidBody& hammer = floating("Hammer", {-3.0f, 0.0f, 0.0f});
    hammer.setVelocity({10.0f, 0.0f, 0.0f});
    run(1.0f);
    CHECK_FALSE(sleeper.isSleeping());
    CHECK(sleeper.owner()->getComponent<RigidBody>()->velocity().x > 0.5f);

    sleeper.sleep();
    CHECK(sleeper.isSleeping());
    sleeper.wakeUp();
    CHECK_FALSE(sleeper.isSleeping());
}

TEST_CASE_FIXTURE(Fixture, "RigidBody: teleport moves the body at once, a query sees it before any step")
{
    RigidBody& body = floating("Body", {0.0f, 0.0f, 0.0f});
    run(0.1f);
    body.teleport({5.0f, 0.0f, 0.0f}, glm::angleAxis(glm::radians(90.0f), Vec3(0.0f, 1.0f, 0.0f)));
    CHECK(static_cast<Node3D*>(body.owner())->worldPosition().x == doctest::Approx(5.0f));
    const auto hit = physics().raycast({0.0f, 0.0f, 0.0f}, kForward, 20.0f);
    REQUIRE(hit);
    CHECK(hit->node.id == body.owner()->id());
    CHECK(hit->distance == doctest::Approx(4.5f).epsilon(0.01));
}

TEST_CASE_FIXTURE(Fixture, "PhysicsWorld: two colliders told to ignore each other pass through")
{
    RigidBody& left = floating("Left", {-2.0f, 0.0f, 0.0f});
    RigidBody& right = floating("Right", {2.0f, 0.0f, 0.0f});
    const ComponentRef<Collider> a{left.owner()->id(), left.owner()->getComponent<SphereCollider>()->id()};
    const ComponentRef<Collider> b{right.owner()->id(), right.owner()->getComponent<SphereCollider>()->id()};
    physics().ignoreCollision(a, b);
    CHECK(physics().collisionIgnored(b, a));
    left.setVelocity({4.0f, 0.0f, 0.0f});
    right.setVelocity({-4.0f, 0.0f, 0.0f});
    run(1.0f);
    CHECK(left.velocity().x == doctest::Approx(4.0f));
    CHECK(static_cast<Node3D*>(left.owner())->worldPosition().x > 1.5f);

    physics().ignoreCollision(a, b, false);
    CHECK_FALSE(physics().collisionIgnored(a, b));
}
