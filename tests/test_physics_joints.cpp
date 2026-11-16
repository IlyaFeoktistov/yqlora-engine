#include "EngineRegistry.hpp"

#include <doctest/doctest.h>

#include <yqlora/core/EngineServices.hpp>
#include <yqlora/input/Input.hpp>
#include <yqlora/physics/BoxCollider.hpp>
#include <yqlora/physics/Joint.hpp>
#include <yqlora/physics/RigidBody.hpp>
#include <yqlora/physics/SphereCollider.hpp>
#include <yqlora/scene/Node3D.hpp>
#include <yqlora/scene/Scene.hpp>
#include <yqlora/scene/Transform3D.hpp>

#include <glm/trigonometric.hpp>

#include <algorithm>
#include <string>
#include <vector>

using namespace yqlora;

namespace {

constexpr float kFrame = 1.0f / 60.0f;
constexpr int kFramesPerSecond = 60;

struct Fixture {
    Fixture()
    {
        scene.setPlaying(true);
        scene.createRoot<Node3D>("Root");
    }

    RigidBody& ball(const std::string& name, Vec3 position, float mass = 1.0f, float radius = 0.1f)
    {
        auto& node = scene.root()->addChild<Node3D>(name);
        node.transform().setPosition(position);
        auto& body = node.addComponent<RigidBody>();
        body.setInterpolation(false);
        body.setMass(mass);
        body.setLinearDamping(0.0f);
        body.setAngularDamping(0.0f);
        node.addComponent<SphereCollider>().setRadius(radius);

        return body;
    }

    RigidBody& crate(const std::string& name, Vec3 position, Vec3 size, float mass = 1.0f)
    {
        auto& node = scene.root()->addChild<Node3D>(name);
        node.transform().setPosition(position);
        auto& body = node.addComponent<RigidBody>();
        body.setInterpolation(false);
        body.setMass(mass);
        body.setLinearDamping(0.0f);
        body.setAngularDamping(0.0f);
        node.addComponent<BoxCollider>().setSize(size);

        return body;
    }

    static Node3D& nodeOf(Component& component)
    {
        return static_cast<Node3D&>(*component.owner());
    }

    void run(float seconds)
    {
        for (int i = 0; i < static_cast<int>(seconds * kFramesPerSecond); ++i) {
            scene.advance(kFrame);
        }
    }

    test::EngineRegistry registry;
    Input input;
    EngineServices services{input, registry};
    Scene scene{services};
};

} // namespace

TEST_CASE_FIXTURE(Fixture, "Joints: a body on a PointJoint to the world swings and keeps its length")
{
    RigidBody& body = ball("Bob", {1.0f, 0.0f, 0.0f});
    nodeOf(body).addComponent<PointJoint>().setAnchor({-1.0f, 0.0f, 0.0f});
    float lowest = 0.0f;
    for (int i = 0; i < kFramesPerSecond * 2; ++i) {
        scene.advance(kFrame);
        const Vec3 position = nodeOf(body).worldPosition();
        CHECK(glm::length(position) == doctest::Approx(1.0f).epsilon(0.02));
        lowest = std::min(lowest, position.y);
    }
    CHECK(lowest < -0.9f);
}

TEST_CASE_FIXTURE(Fixture, "Joints: a HingeJoint with limits stops at them")
{
    RigidBody& door = crate("Door", {0.5f, 0.0f, 0.0f}, {1.0f, 2.0f, 0.1f});
    door.setGravityScale(0.0f);
    auto& hinge = nodeOf(door).addComponent<HingeJoint>();
    hinge.setAnchor({-0.5f, 0.0f, 0.0f});
    hinge.setLimits({.enabled = true, .min = 0.0f, .max = 45.0f});
    scene.advance(kFrame);
    door.setAngularVelocity({0.0f, 3.0f, 0.0f});
    float widest = 0.0f;
    for (int i = 0; i < kFramesPerSecond; ++i) {
        scene.advance(kFrame);
        widest = std::max(widest, std::abs(hinge.angle()));
    }
    CHECK(widest == doctest::Approx(45.0f).epsilon(0.05));
}

TEST_CASE_FIXTURE(Fixture, "Joints: a motor in Velocity reaches its speed, and a new target takes effect")
{
    RigidBody& wheel = crate("Wheel", {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.2f});
    wheel.setGravityScale(0.0f);
    auto& hinge = nodeOf(wheel).addComponent<HingeJoint>();
    hinge.setAxis({0.0f, 0.0f, 1.0f});
    hinge.setMotor(JointMotorMode::Velocity);
    hinge.setMotorTarget(90.0f);
    hinge.setMaxMotorForce(100.0f);
    run(1.0f);
    CHECK(std::abs(wheel.angularVelocity().z) == doctest::Approx(glm::radians(90.0f)).epsilon(0.05));

    hinge.setMotorTarget(-180.0f);
    run(1.0f);
    CHECK(std::abs(wheel.angularVelocity().z) == doctest::Approx(glm::radians(180.0f)).epsilon(0.05));
}

TEST_CASE_FIXTURE(Fixture, "Joints: a motor in Position brings a slider to its target within its limits")
{
    RigidBody& piston = crate("Piston", {0.0f, 0.0f, 0.0f}, Vec3(0.5f));
    piston.setGravityScale(0.0f);
    auto& slider = nodeOf(piston).addComponent<SliderJoint>();
    slider.setLimits({.enabled = true, .min = 0.0f, .max = 1.0f});
    slider.setMotor(JointMotorMode::Position);
    slider.setMotorTarget(2.0f);
    slider.setMaxMotorForce(1000.0f);
    run(2.0f);
    // Цель за пределом: слайдер встаёт на предел.
    CHECK(slider.position() == doctest::Approx(1.0f).epsilon(0.02));
    const Vec3 position = nodeOf(piston).worldPosition();
    CHECK(position.x == doctest::Approx(1.0f).epsilon(0.02));
    CHECK(std::abs(position.y) < 0.01f);
}

TEST_CASE_FIXTURE(Fixture, "Joints: breakForce breaks under a heavy body and calls onBreak once")
{
    RigidBody& weight = ball("Weight", {0.0f, -1.0f, 0.0f}, 10.0f);
    auto& joint = nodeOf(weight).addComponent<FixedJoint>();
    joint.setBreakForce(50.0f);
    int breaks = 0;
    joint.onBreak
        .connect(
            joint,
            [&] {
                ++breaks;
            }
        )
        .release();
    run(1.0f);
    CHECK(breaks == 1);
    CHECK_FALSE(joint.enabled());
    CHECK(nodeOf(weight).worldPosition().y < -3.0f);
}

TEST_CASE_FIXTURE(Fixture, "Joints: a holding joint under its limit does not break and reports its load")
{
    RigidBody& weight = ball("Weight", {0.0f, -1.0f, 0.0f}, 2.0f);
    auto& joint = nodeOf(weight).addComponent<FixedJoint>();
    joint.setBreakForce(100.0f);
    run(1.0f);
    CHECK(joint.enabled());
    // Держит вес: m * g.
    CHECK(joint.force() == doctest::Approx(2.0f * 9.81f).epsilon(0.05));
}

TEST_CASE("Joints: a chain of 20 links with a load ten times heavier holds 1 percent with raised solver steps")
{
    // Растяжение цепи: длина от точки крепления к миру до груза к длине покоя.
    const auto stretch = [](int solverSteps) {
        constexpr int kLinks = 20;
        constexpr float kSpacing = 0.25f;
        constexpr float kLinkMass = 1.0f;
        constexpr float kLoadMass = 10.0f;
        Fixture fixture;
        RigidBody* previous = nullptr;
        for (int i = 1; i <= kLinks + 1; ++i) {
            const float mass = i == kLinks + 1 ? kLoadMass : kLinkMass;
            RigidBody& link = fixture.ball("Link", {0.0f, -kSpacing * static_cast<float>(i), 0.0f}, mass);
            auto& joint = Fixture::nodeOf(link).addComponent<PointJoint>();
            joint.setAnchor({0.0f, kSpacing * 0.5f, 0.0f});
            joint.setSolverStepsOverride(solverSteps);
            if (previous) {
                joint.setConnectedBody(NodeRef{Fixture::nodeOf(*previous).id()});
            }
            previous = &link;
        }
        fixture.run(3.0f);
        const float top = -kSpacing * 0.5f;
        const float rest = kSpacing * static_cast<float>(kLinks + 1) + top;

        return (top - Fixture::nodeOf(*previous).worldPosition().y) / rest;
    };
    constexpr int kRaisedSteps = 20;
    const float raised = stretch(kRaisedSteps);
    CHECK(raised < 1.01f);
    CHECK(raised < stretch(0));
}

TEST_CASE_FIXTURE(Fixture, "Joints: a DistanceJoint keeps the body within its range")
{
    RigidBody& body = ball("Body", {0.0f, -0.5f, 0.0f});
    auto& rope = nodeOf(body).addComponent<DistanceJoint>();
    rope.setConnectedAnchor({0.0f, 0.0f, 0.0f});
    rope.setMinDistance(0.0f);
    rope.setMaxDistance(2.0f);
    run(2.0f);
    CHECK(glm::length(nodeOf(body).worldPosition()) == doctest::Approx(2.0f).epsilon(0.02));
}

TEST_CASE_FIXTURE(Fixture, "Joints: connected bodies do not collide unless collideConnected")
{
    const auto separation = [&](bool collide) {
        Fixture fixture;
        RigidBody& first = fixture.crate("First", {0.0f, 0.0f, 0.0f}, Vec3(1.0f));
        RigidBody& second = fixture.crate("Second", {0.5f, 0.0f, 0.0f}, Vec3(1.0f));
        first.setGravityScale(0.0f);
        second.setGravityScale(0.0f);
        auto& joint = nodeOf(second).addComponent<DistanceJoint>();
        joint.setConnectedBody(NodeRef{nodeOf(first).id()});
        joint.setMinDistance(0.0f);
        joint.setMaxDistance(5.0f);
        joint.setCollideConnected(collide);
        fixture.run(0.5f);

        return nodeOf(second).worldPosition().x - nodeOf(first).worldPosition().x;
    };
    CHECK(separation(false) == doctest::Approx(0.5f).epsilon(0.02));
    CHECK(separation(true) > 0.9f);
}

TEST_CASE("Joints: a scene that does not run builds no joints")
{
    Fixture fixture;
    fixture.scene.setPlaying(false);
    RigidBody& body = fixture.ball("Bob", {1.0f, 0.0f, 0.0f});
    auto& joint = Fixture::nodeOf(body).addComponent<PointJoint>();
    fixture.scene.updateTransforms();
    CHECK(joint.force() == 0.0f);
}
