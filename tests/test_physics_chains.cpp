#include "EngineRegistry.hpp"

#include <doctest/doctest.h>

#include <yqlora/core/EngineServices.hpp>
#include <yqlora/input/Input.hpp>
#include <yqlora/physics/BoxCollider.hpp>
#include <yqlora/physics/PhysicsChain.hpp>
#include <yqlora/physics/PhysicsWorld.hpp>
#include <yqlora/physics/RigidBody.hpp>
#include <yqlora/physics/SphereCollider.hpp>
#include <yqlora/scene/Node3D.hpp>
#include <yqlora/scene/Scene.hpp>
#include <yqlora/scene/Transform3D.hpp>

#include <glm/trigonometric.hpp>

#include <algorithm>
#include <cmath>
#include <optional>
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

    // Пол: верх - на высоте top, середина - в center по XZ.
    Node3D& floor(Vec3 size = {40.0f, 1.0f, 40.0f}, Vec3 center = Vec3(0.0f), float top = 0.0f)
    {
        auto& node = scene.root()->addChild<Node3D>("Floor");
        node.transform().setPosition({center.x, top - size.y * 0.5f, center.z});
        node.addComponent<BoxCollider>().setSize(size);

        return node;
    }

    PhysicsChain& chain(Vec3 position, int segments, float length, float radius)
    {
        auto& node = scene.root()->addChild<Node3D>("Chain");
        node.transform().setPosition(position);
        auto& chain = node.addComponent<PhysicsChain>();
        chain.setSegments(segments);
        chain.setLength(length);
        chain.setRadius(radius);

        return chain;
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

// Наименьшее расстояние между центрами несоседних звеньев.
float closestApart(const PhysicsChain& chain)
{
    float closest = 1e9f;
    for (int i = 0; i < chain.segments(); ++i) {
        for (int j = i + 2; j < chain.segments(); ++j) {
            closest = std::min(closest, glm::length(chain.linkPosition(i) - chain.linkPosition(j)));
        }
    }

    return closest;
}

// Сдвиг середины цепи вдоль -Z за seconds, когда по ней бежит волна изгибов
// вбок от головы к хвосту.
float crawl(float sideGrip, float forwardGrip, float seconds)
{
    Fixture fixture;
    fixture.floor();
    PhysicsChain& snake = fixture.chain({0.0f, 0.1f, 0.0f}, 12, 2.4f, 0.08f);
    snake.setMass(3.0f);
    snake.setBendLimit(60.0f);
    snake.setMaxTorque(30.0f);
    snake.setSideGrip(sideGrip);
    snake.setForwardGrip(forwardGrip);
    fixture.run(0.5f);
    const auto middle = [&] {
        Vec3 sum(0.0f);
        for (int i = 0; i < snake.segments(); ++i) {
            sum += snake.linkPosition(i);
        }

        return sum / static_cast<float>(snake.segments());
    };
    const Vec3 start = middle();
    constexpr float kAmplitude = 35.0f;
    constexpr float kFrequency = 1.2f;
    constexpr float kWavesAlongBody = 1.0f;
    std::vector<Vec2> bends(static_cast<std::size_t>(snake.segments() - 1));
    for (int frame = 0; frame < static_cast<int>(seconds * kFramesPerSecond); ++frame) {
        const float time = static_cast<float>(frame) * kFrame;
        for (std::size_t i = 0; i < bends.size(); ++i) {
            const float phase = glm::two_pi<float>()
                                * (kFrequency * time
                                   - kWavesAlongBody * static_cast<float>(i) / static_cast<float>(bends.size()));
            bends[i] = {kAmplitude * std::sin(phase), 0.0f};
        }
        snake.setBends(bends);
        fixture.scene.advance(kFrame);
    }

    return -(middle() - start).z;
}

} // namespace

TEST_CASE_FIXTURE(Fixture, "Chains: a chain of 10 links dropped on the ground lies along it")
{
    floor();
    PhysicsChain& rope = chain({0.0f, 1.0f, 0.0f}, 10, 2.0f, 0.1f);
    rope.relax();
    run(3.0f);
    for (int i = 0; i < rope.segments(); ++i) {
        CAPTURE(i);
        const Vec3 position = rope.linkPosition(i);
        CHECK(position.y == doctest::Approx(0.1f).epsilon(0.3));
        CHECK(position.z == doctest::Approx(0.1f + 0.2f * static_cast<float>(i)).epsilon(0.1));
    }
}

TEST_CASE_FIXTURE(Fixture, "Chains: links of a chain falling on itself do not pass through each other")
{
    floor();
    PhysicsChain& rope = chain({0.0f, 3.0f, 0.0f}, 10, 2.0f, 0.1f);
    // Звенья - вниз: цепь падает на хвост и складывается в кучу.
    nodeOf(rope).transform().setRotation(glm::angleAxis(glm::radians(80.0f), Vec3(1.0f, 0.0f, 0.0f)));
    rope.setBendLimit(120.0f);
    rope.relax();
    float closest = 1e9f;
    for (int i = 0; i < kFramesPerSecond * 3; ++i) {
        scene.advance(kFrame);
        closest = std::min(closest, closestApart(rope));
    }
    // Два радиуса - касание; с допуском проникновения решателя.
    CHECK(closest > 0.2f * 0.8f);
    for (int i = 0; i < rope.segments(); ++i) {
        CHECK(rope.linkPosition(i).y > 0.05f);
    }
}

TEST_CASE_FIXTURE(Fixture, "Chains: a bend beyond bendLimit stops at the limit")
{
    PhysicsChain& tail = chain({0.0f, 5.0f, 0.0f}, 2, 1.0f, 0.05f);
    tail.setBendLimit(30.0f);
    tail.setMaxTorque(200.0f);
    tail.setStiffness(500.0f);
    tail.setBend(0, 80.0f, 0.0f);
    float widest = 0.0f;
    for (int i = 0; i < kFramesPerSecond; ++i) {
        scene.advance(kFrame);
        widest = std::max(widest, std::abs(tail.bend(0).x));
    }
    CHECK(widest == doctest::Approx(30.0f).epsilon(0.1));
}

TEST_CASE_FIXTURE(Fixture, "Chains: muscles reach the target bend")
{
    PhysicsChain& tail = chain({0.0f, 5.0f, 0.0f}, 3, 1.5f, 0.05f);
    tail.setMaxTorque(100.0f);
    tail.setBends({{20.0f, 0.0f}, {-10.0f, 15.0f}});
    run(1.5f);
    CHECK(tail.bend(0).x == doctest::Approx(20.0f).epsilon(0.1));
    CHECK(tail.bend(0).y == doctest::Approx(0.0f).scale(10.0f).epsilon(0.2));
    CHECK(tail.bend(1).x == doctest::Approx(-10.0f).epsilon(0.1));
    CHECK(tail.bend(1).y == doctest::Approx(15.0f).epsilon(0.1));
    CHECK(tail.targetBend(1) == Vec2(-10.0f, 15.0f));
}

TEST_CASE("Chains: a travelling wave moves the chain forward only when side grip exceeds forward grip")
{
    const float snake = crawl(0.9f, 0.05f, 4.0f);
    const float even = crawl(0.5f, 0.5f, 4.0f);
    MESSAGE("snake ", snake, " m, even grip ", even, " m");
    CHECK(snake > 0.4f);
    CHECK(std::abs(even) < snake * 0.25f);
}

TEST_CASE_FIXTURE(Fixture, "Chains: relax lets a chain go limp")
{
    // Голова лежит на краю, хвост висит над пустотой: мышцы держат его прямо.
    floor({4.0f, 1.0f, 2.0f}, {0.0f, 0.0f, 0.0f});
    PhysicsChain& tail = chain({0.0f, 0.06f, 0.0f}, 8, 1.6f, 0.05f);
    tail.setMass(2.0f);
    tail.setMaxTorque(100.0f);
    tail.setStiffness(200.0f);
    run(1.0f);
    const int last = tail.segments() - 1;
    CHECK(tail.linkPosition(last).y > -0.1f);
    CHECK_FALSE(tail.relaxed());

    tail.relax();
    CHECK(tail.relaxed());
    run(1.0f);
    CHECK(tail.linkPosition(last).y < -0.3f);
}

TEST_CASE_FIXTURE(Fixture, "Chains: the chain node follows the head and segment nodes follow their links")
{
    floor();
    PhysicsChain& rope = chain({0.0f, 1.0f, 0.0f}, 4, 2.0f, 0.1f);
    Node3D& node = nodeOf(rope);
    std::vector<Node3D*> segments;
    for (int i = 0; i < rope.segments(); ++i) {
        auto& segment = node.addChild<Node3D>("Segment " + std::to_string(i));
        // На полметра выше центра своего звена.
        segment.transform().setPosition({0.0f, 0.5f, 0.25f + 0.5f * static_cast<float>(i)});
        segments.push_back(&segment);
    }
    rope.setBend(0, 30.0f, 0.0f);
    run(2.0f);
    // Узел цепи - в начале головы, вдоль её оси.
    const Vec3 headAxis = glm::normalize(rope.linkPosition(1) - rope.linkPosition(0));
    CHECK(glm::length(node.worldPosition() - (rope.linkPosition(0) - headAxis * 0.25f)) < 0.1f);
    for (int i = 0; i < rope.segments(); ++i) {
        CAPTURE(i);
        const Vec3 expected = rope.linkPosition(i) + Vec3(0.0f, 0.5f, 0.0f);
        CHECK(glm::length(segments[static_cast<std::size_t>(i)]->worldPosition() - expected) < 0.1f);
    }
}

TEST_CASE_FIXTURE(Fixture, "Chains: moving the chain node by code carries the whole chain")
{
    PhysicsChain& rope = chain({0.0f, 5.0f, 0.0f}, 4, 2.0f, 0.1f);
    scene.advance(kFrame);
    const Vec3 before = rope.linkPosition(3) - rope.linkPosition(0);
    nodeOf(rope).setWorldPosition({10.0f, 5.0f, 0.0f});
    scene.advance(kFrame);
    CHECK(rope.linkPosition(0).x == doctest::Approx(10.0f).epsilon(0.01));
    CHECK(glm::length(rope.linkPosition(3) - rope.linkPosition(0) - before) < 0.05f);
}

TEST_CASE_FIXTURE(Fixture, "Chains: contacts and queries name the link")
{
    floor();
    PhysicsChain& rope = chain({0.0f, 0.1f, 0.0f}, 5, 2.5f, 0.1f);
    run(0.5f);
    auto& ball = scene.root()->addChild<Node3D>("Ball");
    ball.transform().setPosition({0.0f, 1.0f, 1.25f});
    auto& body = ball.addComponent<RigidBody>();
    body.setInterpolation(false);
    ball.addComponent<SphereCollider>().setRadius(0.1f);
    std::optional<Collision> chainSide;
    std::optional<Collision> ballSide;
    rope.onCollisionEnter
        .connect(
            rope,
            [&](const Collision& collision) {
                if (!chainSide && collision.other.id == ball.id()) {
                    chainSide = collision;
                }
            }
        )
        .release();
    body.onCollisionEnter
        .connect(
            body,
            [&](const Collision& collision) {
                if (!ballSide && collision.other.id == nodeOf(rope).id()) {
                    ballSide = collision;
                }
            }
        )
        .release();
    run(1.0f);
    REQUIRE(chainSide);
    REQUIRE(ballSide);
    CHECK(chainSide->link == 2);
    CHECK(chainSide->otherLink == kNoLink);
    CHECK(ballSide->link == kNoLink);
    CHECK(ballSide->otherLink == 2);

    const auto hit = scene.system<PhysicsWorld>()->raycast({0.0f, 3.0f, 0.3f}, {0.0f, -1.0f, 0.0f}, 5.0f);
    REQUIRE(hit);
    CHECK(hit->node.id == nodeOf(rope).id());
    CHECK(hit->link == 0);
}
