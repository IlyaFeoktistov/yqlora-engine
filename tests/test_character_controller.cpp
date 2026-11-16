#include "EngineRegistry.hpp"

#include <doctest/doctest.h>

#include <yqlora/core/EngineServices.hpp>
#include <yqlora/input/Input.hpp>
#include <yqlora/physics/BoxCollider.hpp>
#include <yqlora/physics/CharacterController.hpp>
#include <yqlora/physics/PhysicsWorld.hpp>
#include <yqlora/physics/RigidBody.hpp>
#include <yqlora/scene/Node3D.hpp>
#include <yqlora/scene/Scene.hpp>
#include <yqlora/scene/Transform3D.hpp>

#include <cmath>
#include <string>
#include <vector>

using namespace yqlora;

namespace {

constexpr float kFrame = 1.0f / 60.0f;
constexpr int kFramesPerSecond = 60;
constexpr float kGravity = -9.81f;
const Vec3 kForward{1.0f, 0.0f, 0.0f};

// Игра в тестах: скорость по земле, гравитация, пока в воздухе.
struct Walker {
    CharacterController& controller;
    float fall = 0.0f;

    void step(const Vec3& horizontal)
    {
        fall = controller.isGrounded() ? 0.0f : fall + kGravity * kFrame;
        controller.move(horizontal + Vec3(0.0f, fall, 0.0f), kFrame);
    }
};

struct Fixture {
    Fixture()
    {
        scene.setPlaying(true);
        scene.createRoot<Node3D>("Root");
    }

    Node3D& box(const std::string& name, Vec3 center, Vec3 size, Vec3 rotationDegrees = Vec3(0.0f))
    {
        auto& node = scene.root()->addChild<Node3D>(name);
        node.transform().setPosition(center);
        node.transform().setRotationDegrees(rotationDegrees);
        node.addComponent<BoxCollider>().setSize(size);

        return node;
    }

    // Пол с верхом на высоте top.
    Node3D& floor(const std::string& name, Vec3 center, Vec3 size, float top)
    {
        return box(name, {center.x, top - size.y * 0.5f, center.z}, size);
    }

    CharacterController& character(const std::string& name, Vec3 position)
    {
        auto& node = scene.root()->addChild<Node3D>(name);
        node.transform().setPosition(position);

        return node.addComponent<CharacterController>();
    }

    Node3D& nodeOf(CharacterController& controller)
    {
        return static_cast<Node3D&>(*controller.owner());
    }

    // Кадры: шаги физики, затем движение персонажа, как в onUpdate игры.
    void walk(Walker& walker, const Vec3& horizontal, float seconds)
    {
        for (int i = 0; i < static_cast<int>(seconds * kFramesPerSecond); ++i) {
            scene.advance(kFrame);
            walker.step(horizontal);
        }
    }

    test::EngineRegistry registry;
    Input input;
    EngineServices services{input, registry};
    Scene scene{services};
};

float horizontalSpeed(const Vec3& velocity)
{
    return std::sqrt(velocity.x * velocity.x + velocity.z * velocity.z);
}

} // namespace

TEST_CASE_FIXTURE(Fixture, "CharacterController: walks onto a step below stepHeight and stops at a higher one")
{
    floor("Floor", {0.0f, 0.0f, 0.0f}, {40.0f, 1.0f, 40.0f}, 0.0f);
    floor("Low", {3.0f, 0.0f, 0.0f}, {2.0f, 1.0f, 4.0f}, 0.2f);
    floor("High", {3.0f, 0.0f, 10.0f}, {2.0f, 1.0f, 4.0f}, 0.6f);
    Walker low{character("Low walker", {0.0f, 0.0f, 0.0f})};
    Walker high{character("High walker", {0.0f, 0.0f, 10.0f})};
    walk(low, kForward * 3.0f, 1.0f);
    walk(high, kForward * 3.0f, 1.0f);
    const Vec3 onLow = nodeOf(low.controller).worldPosition();
    CHECK(onLow.x > 2.5f);
    CHECK(onLow.y == doctest::Approx(0.2f).epsilon(0.05));
    const Vec3 atHigh = nodeOf(high.controller).worldPosition();
    CHECK(atHigh.x < 2.0f);
    CHECK(atHigh.y < 0.1f);
}

TEST_CASE("CharacterController: running down stairs keeps him on the ground only with stickToGround")
{
    const auto groundedShare = [](bool stick) {
        Fixture fixture;
        constexpr int kSteps = 8;
        constexpr float kRise = 0.2f;
        constexpr float kRun = 0.4f;
        constexpr float kTop = kSteps * kRise;
        fixture.floor("Landing", {-2.0f, 0.0f, 0.0f}, {4.0f, 1.0f, 4.0f}, kTop);
        for (int i = 0; i < kSteps; ++i) {
            const float top = kTop - kRise * static_cast<float>(i + 1);
            fixture.floor("Step", {kRun * (static_cast<float>(i) + 0.5f), 0.0f, 0.0f}, {kRun, 1.0f, 4.0f}, top);
        }
        fixture.floor("Floor", {10.0f, 0.0f, 0.0f}, {14.0f, 1.0f, 4.0f}, 0.0f);
        CharacterController& controller = fixture.character("Runner", {-1.0f, kTop, 0.0f});
        controller.setStickToGround(stick);
        Walker walker{controller};
        fixture.walk(walker, Vec3(0.0f), 0.2f);
        int frames = 0;
        int grounded = 0;
        for (int i = 0; i < kFramesPerSecond * 2; ++i) {
            fixture.scene.advance(kFrame);
            walker.step(kForward * 3.0f);
            const float x = fixture.nodeOf(controller).worldPosition().x;
            if (x > 0.2f && x < kRun * kSteps - 0.2f) {
                ++frames;
                grounded += controller.isGrounded() ? 1 : 0;
            }
        }
        REQUIRE(frames > 0);

        return static_cast<float>(grounded) / static_cast<float>(frames);
    };
    CHECK(groundedShare(true) == doctest::Approx(1.0f));
    CHECK(groundedShare(false) < 0.8f);
}

TEST_CASE_FIXTURE(Fixture, "CharacterController: KeepHorizontal keeps the ground speed up, down and on the flat")
{
    constexpr float kSlope = 20.0f;
    constexpr float kSpeed = 3.0f;
    box("Ramp", {0.0f, 0.0f, 0.0f}, {60.0f, 0.2f, 6.0f}, {0.0f, 0.0f, kSlope});
    floor("Flat", {0.0f, 0.0f, 20.0f}, {20.0f, 1.0f, 6.0f}, 0.0f);
    Walker onRamp{character("On ramp", {0.0f, 0.5f, 0.0f})};
    Walker onFlat{character("On flat", {0.0f, 0.1f, 20.0f})};
    walk(onRamp, Vec3(0.0f), 0.5f);
    walk(onFlat, Vec3(0.0f), 0.5f);
    REQUIRE(onRamp.controller.isGrounded());

    walk(onFlat, kForward * kSpeed, 0.5f);
    const float flat = horizontalSpeed(onFlat.controller.velocity());
    walk(onRamp, kForward * kSpeed, 0.5f);
    const float up = horizontalSpeed(onRamp.controller.velocity());
    CHECK(nodeOf(onRamp.controller).worldPosition().y > 0.4f);
    walk(onRamp, -kForward * kSpeed, 0.5f);
    const float down = horizontalSpeed(onRamp.controller.velocity());
    CHECK(onRamp.controller.isGrounded());
    CHECK(flat == doctest::Approx(kSpeed).epsilon(0.05));
    CHECK(up == doctest::Approx(kSpeed).epsilon(0.05));
    CHECK(down == doctest::Approx(kSpeed).epsilon(0.05));

    // Вдоль склона - по плану медленнее, на косинус угла.
    onRamp.controller.setSlopeSpeed(SlopeSpeed::AlongSurface);
    walk(onRamp, kForward * kSpeed, 0.5f);
    const float along = horizontalSpeed(onRamp.controller.velocity());
    CHECK(along == doctest::Approx(kSpeed * std::cos(kSlope * 3.14159265f / 180.0f)).epsilon(0.05));
}

TEST_CASE_FIXTURE(Fixture, "CharacterController: a drop deeper than stickDistance is a fall")
{
    floor("Ledge", {-2.0f, 0.0f, 0.0f}, {4.0f, 1.0f, 4.0f}, 0.0f);
    floor("Below", {5.0f, 0.0f, 0.0f}, {10.0f, 1.0f, 4.0f}, -2.0f);
    Walker walker{character("Walker", {-1.0f, 0.0f, 0.0f})};
    walk(walker, Vec3(0.0f), 0.2f);
    bool fell = false;
    for (int i = 0; i < kFramesPerSecond * 2; ++i) {
        scene.advance(kFrame);
        walker.step(kForward * 3.0f);
        const Vec3 position = nodeOf(walker.controller).worldPosition();
        if (position.x > 0.5f && position.y < -0.1f && position.y > -1.9f) {
            fell = fell || !walker.controller.isGrounded();
        }
    }
    CHECK(fell);
    CHECK(nodeOf(walker.controller).worldPosition().y == doctest::Approx(-2.0f).epsilon(0.05));
    CHECK(walker.controller.isGrounded());
}

TEST_CASE_FIXTURE(Fixture, "CharacterController: resize refuses under a low ceiling")
{
    floor("Floor", {0.0f, 0.0f, 0.0f}, {40.0f, 1.0f, 40.0f}, 0.0f);
    box("Ceiling", {0.0f, 1.45f, 0.0f}, {4.0f, 0.5f, 4.0f});
    CharacterController& under = character("Under", {0.0f, 0.0f, 0.0f});
    under.setHeight(1.0f);
    under.setCenter({0.0f, 0.5f, 0.0f});
    CharacterController& open = character("Open", {10.0f, 0.0f, 0.0f});
    open.setHeight(1.0f);
    open.setCenter({0.0f, 0.5f, 0.0f});
    scene.advance(kFrame);

    CHECK_FALSE(under.resize(1.8f));
    CHECK(under.height() == doctest::Approx(1.0f));
    CHECK(under.resize(0.8f));
    CHECK(under.center().y == doctest::Approx(0.4f));
    CHECK(open.resize(1.8f));
    CHECK(open.center().y == doctest::Approx(0.9f));
}

TEST_CASE_FIXTURE(Fixture, "CharacterController: slides along a wall it walks into at an angle")
{
    floor("Floor", {0.0f, 0.0f, 0.0f}, {40.0f, 1.0f, 40.0f}, 0.0f);
    box("Wall", {0.0f, 1.0f, 1.5f}, {40.0f, 2.0f, 1.0f});
    Walker walker{character("Walker", {0.0f, 0.0f, 0.0f})};
    walk(walker, glm::normalize(Vec3(1.0f, 0.0f, 1.0f)) * 3.0f, 1.0f);
    const Vec3 position = nodeOf(walker.controller).worldPosition();
    CHECK(position.x > 1.5f);
    CHECK(position.z < 1.0f - 0.39f);
}

TEST_CASE_FIXTURE(Fixture, "CharacterController: does not climb a slope steeper than its limit")
{
    floor("Floor", {0.0f, 0.0f, 0.0f}, {40.0f, 1.0f, 40.0f}, 0.0f);
    box("Steep", {4.0f, 0.0f, 0.0f}, {6.0f, 0.2f, 6.0f}, {0.0f, 0.0f, 60.0f});
    Walker walker{character("Walker", {0.0f, 0.0f, 0.0f})};
    walk(walker, kForward * 3.0f, 2.0f);
    CHECK(nodeOf(walker.controller).worldPosition().y < 0.5f);
}

TEST_CASE_FIXTURE(Fixture, "CharacterController: rides a moving platform")
{
    Node3D& platform = floor("Platform", {0.0f, 0.0f, 0.0f}, {4.0f, 0.5f, 4.0f}, 0.0f);
    auto& body = platform.addComponent<RigidBody>();
    body.setMotion(BodyMotion::Kinematic);
    Walker walker{character("Rider", {0.0f, 0.0f, 0.0f})};
    walk(walker, Vec3(0.0f), 0.2f);
    const Vec3 start = platform.worldPosition();
    for (int i = 0; i < kFramesPerSecond; ++i) {
        body.moveTo(start + kForward * (static_cast<float>(i + 1) * kFrame));
        scene.advance(kFrame);
        walker.step(Vec3(0.0f));
    }
    const float carried = nodeOf(walker.controller).worldPosition().x;
    CHECK(carried == doctest::Approx(platform.worldPosition().x - start.x).epsilon(0.15));
    CHECK(walker.controller.groundVelocity().x > 0.5f);
}

TEST_CASE("CharacterController: pushes a light box and not a heavy one")
{
    const auto pushed = [](float mass) {
        Fixture fixture;
        fixture.floor("Floor", {0.0f, 0.0f, 0.0f}, {40.0f, 1.0f, 40.0f}, 0.0f);
        Node3D& crate = fixture.box("Crate", {1.5f, 0.5f, 0.0f}, Vec3(1.0f));
        crate.addComponent<RigidBody>().setMass(mass);
        Walker walker{fixture.character("Pusher", {0.0f, 0.0f, 0.0f})};
        fixture.walk(walker, kForward * 2.0f, 1.5f);

        return crate.worldPosition().x - 1.5f;
    };
    CHECK(pushed(5.0f) > 0.3f);
    CHECK(pushed(2000.0f) < 0.05f);
}

TEST_CASE_FIXTURE(Fixture, "CharacterController: onControllerHit names what he touched, once per new touch")
{
    floor("Floor", {0.0f, 0.0f, 0.0f}, {40.0f, 1.0f, 40.0f}, 0.0f);
    Node3D& wall = box("Wall", {2.0f, 1.0f, 0.0f}, {1.0f, 2.0f, 4.0f});
    CharacterController& controller = character("Walker", {0.0f, 0.0f, 0.0f});
    std::vector<NodeId> hits;
    controller.onControllerHit
        .connect(
            controller,
            [&](const NodeRef& other) {
                hits.push_back(other.id);
            }
        )
        .release();
    Walker walker{controller};
    walk(walker, kForward * 3.0f, 1.0f);
    CHECK(std::ranges::count(hits, wall.id()) == 1);
}

TEST_CASE_FIXTURE(Fixture, "CharacterController: triggers and queries see him as his node")
{
    floor("Floor", {0.0f, 0.0f, 0.0f}, {40.0f, 1.0f, 40.0f}, 0.0f);
    Node3D& zone = box("Zone", {3.0f, 1.0f, 0.0f}, {1.0f, 2.0f, 4.0f});
    zone.getComponent<BoxCollider>()->setTrigger(true);
    CharacterController& controller = character("Walker", {0.0f, 0.0f, 0.0f});
    std::vector<NodeId> entered;
    zone.getComponent<BoxCollider>()
        ->onTriggerEnter
        .connect(
            controller,
            [&](const NodeRef& other) {
                entered.push_back(other.id);
            }
        )
        .release();
    scene.advance(kFrame);
    const auto hit = scene.system<PhysicsWorld>()->raycast({-3.0f, 1.0f, 0.0f}, kForward, 10.0f);
    REQUIRE(hit);
    CHECK(hit->node.id == controller.owner()->id());

    Walker walker{controller};
    walk(walker, kForward * 3.0f, 1.5f);
    CHECK(entered == std::vector<NodeId>{controller.owner()->id()});
}
