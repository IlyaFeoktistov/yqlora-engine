#include "EngineRegistry.hpp"

#include <doctest/doctest.h>

#include <yqlora/core/EngineServices.hpp>
#include <yqlora/debug/DebugDraw.hpp>
#include <yqlora/debug/DebugLines.hpp>
#include <yqlora/input/Input.hpp>
#include <yqlora/physics/BoxCollider.hpp>
#include <yqlora/physics/PhysicsDebug.hpp>
#include <yqlora/physics/PhysicsWorld.hpp>
#include <yqlora/physics/RigidBody.hpp>
#include <yqlora/physics/SphereCollider.hpp>
#include <yqlora/render/FrameBuilder.hpp>
#include <yqlora/render/FrameExtract.hpp>
#include <yqlora/scene/Node3D.hpp>
#include <yqlora/scene/Scene.hpp>
#include <yqlora/scene/Transform3D.hpp>

#include <algorithm>
#include <span>
#include <string>

using namespace yqlora;

namespace {

constexpr float kFrame = 1.0f / 60.0f;
constexpr int kFramesPerSecond = 60;
constexpr float kAspect = 1.0f;
constexpr float kNear = 1e-3f;
const Quat kNoRotation{1.0f, 0.0f, 0.0f, 0.0f};
const DebugChannels kGame{std::string(kDefaultDebugChannel)};

struct Fixture {
    explicit Fixture(bool playing = true)
    {
        scene.setPlaying(playing);
        scene.createRoot<Node3D>("Root");
    }

    Node3D& box(const std::string& name, Vec3 position, Vec3 scale = Vec3(1.0f))
    {
        auto& node = scene.root()->addChild<Node3D>(name);
        node.transform().setPosition(position);
        node.transform().setScale(scale);
        node.addComponent<BoxCollider>();

        return node;
    }

    RigidBody& ball(const std::string& name, Vec3 position)
    {
        auto& node = scene.root()->addChild<Node3D>(name);
        node.transform().setPosition(position);
        auto& body = node.addComponent<RigidBody>();
        body.setInterpolation(false);
        node.addComponent<SphereCollider>();

        return body;
    }

    void run(float seconds)
    {
        for (int i = 0; i < static_cast<int>(seconds * kFramesPerSecond); ++i) {
            scene.advance(kFrame);
        }
    }

    DebugDraw& debug()
    {
        return *scene.system<DebugDraw>();
    }

    DebugLines lines(const DebugChannels& channels)
    {
        scene.updateTransforms();
        DebugLines result;
        scene.drawDebug(result, channels);

        return result;
    }

    test::EngineRegistry registry;
    Input input;
    EngineServices services{input, registry};
    Scene scene{services};
};

bool hasVertexNear(std::span<const DebugVertex> vertices, const Vec3& position)
{
    return std::ranges::any_of(vertices, [&](const DebugVertex& vertex) {
        return glm::length(vertex.position - position) < kNear;
    });
}

} // namespace

TEST_CASE("DebugLines: shapes split into segments, overlay apart from depth-tested")
{
    DebugLines lines;
    lines.box(Vec3(0.0f), Vec3(1.0f), kNoRotation, {});
    // 12 рёбер по две вершины.
    CHECK(lines.tested().size() == 24);
    CHECK(lines.overlay().empty());
    lines.line(Vec3(0.0f), Vec3(1.0f), {.depthTest = false});
    CHECK(lines.overlay().size() == 2);
    CHECK(hasVertexNear(lines.tested(), Vec3(1.0f, -1.0f, 1.0f)));
}

TEST_CASE_FIXTURE(Fixture, "DebugDraw: a timed primitive lives its seconds of game time and goes")
{
    debug().setViewerChannels(kGame);
    scene.advance(kFrame);
    debug().line(Vec3(0.0f), Vec3(1.0f), {.duration = 0.5f});
    run(0.4f);
    CHECK_FALSE(lines(kGame).empty());
    run(0.2f);
    CHECK(lines(kGame).empty());
}

TEST_CASE_FIXTURE(Fixture, "DebugDraw: a one-frame primitive goes with the next frame")
{
    debug().setViewerChannels(kGame);
    scene.advance(kFrame);
    debug().point(Vec3(0.0f), 1.0f);
    CHECK_FALSE(lines(kGame).empty());
    scene.advance(kFrame);
    CHECK(lines(kGame).empty());
}

TEST_CASE_FIXTURE(Fixture, "DebugDraw: a channel nobody shows collects nothing")
{
    debug().line(Vec3(0.0f), Vec3(1.0f));
    CHECK(lines(kGame).empty());
    CHECK_FALSE(debug().collecting(kDefaultDebugChannel));
    // Канал виден в списке, хотя и не собирался.
    CHECK(scene.debugChannels().contains(kDefaultDebugChannel));

    // Игра включила свою отладку: её канал по умолчанию собирается, чужой - нет.
    debug().setEnabled(true);
    CHECK(debug().gameChannels() == kGame);
    debug().line(Vec3(0.0f), Vec3(1.0f), {.channel = "ai"});
    CHECK(lines({"ai"}).empty());
    debug().showChannel("ai");
    debug().line(Vec3(0.0f), Vec3(1.0f), {.channel = "ai"});
    CHECK_FALSE(lines({"ai"}).empty());
    // Показанный канал не тянет за собой чужие.
    CHECK(lines(kGame).empty());
}

TEST_CASE("DebugDraw: a scene that does not run collects nothing")
{
    Fixture fixture(false);
    fixture.debug().setViewerChannels(kGame);
    fixture.debug().line(Vec3(0.0f), Vec3(1.0f));
    CHECK(fixture.lines(kGame).empty());
}

TEST_CASE_FIXTURE(Fixture, "DebugDraw: a query lays its ray and hit point while the Queries channel is shown")
{
    const DebugChannels queries{std::string(kQueriesDebugChannel)};
    box("Wall", {3.0f, 0.0f, 0.0f});
    scene.advance(kFrame);
    // Пока канал никто не показывает, запрос ничего не кладёт.
    REQUIRE(scene.system<PhysicsWorld>()->raycast(Vec3(0.0f), Vec3(1.0f, 0.0f, 0.0f), 10.0f));
    CHECK(lines(queries).empty());
    debug().setViewerChannels(queries);
    const auto hit = scene.system<PhysicsWorld>()->raycast(Vec3(0.0f), Vec3(1.0f, 0.0f, 0.0f), 10.0f);
    REQUIRE(hit);
    const DebugLines drawn = lines(queries);
    CHECK(hasVertexNear(drawn.tested(), Vec3(0.0f)));
    CHECK(hasVertexNear(drawn.tested(), hit->point));
    // Остаток пути до maxDistance - промахом.
    CHECK(hasVertexNear(drawn.tested(), Vec3(10.0f, 0.0f, 0.0f)));
}

TEST_CASE_FIXTURE(Fixture, "DebugDraw: a contact gives its point and normal")
{
    box("Floor", {0.0f, -0.5f, 0.0f}, {10.0f, 1.0f, 10.0f});
    ball("Ball", {0.0f, 1.0f, 0.0f});
    const DebugChannels contacts{std::string(kContactsDebugChannel)};
    CHECK(lines(contacts).empty());
    run(2.0f);
    const DebugLines drawn = lines(contacts);
    REQUIRE_FALSE(drawn.empty());
    // Шар лежит на полу: касание - на его верхней грани, нормаль вверх.
    const bool onFloor = std::ranges::any_of(drawn.tested(), [](const DebugVertex& vertex) {
        return std::abs(vertex.position.y) < 0.05f && std::abs(vertex.position.x) < 0.1f;
    });
    CHECK(onFloor);
}

TEST_CASE("DebugDraw: colliders show in a scene that does not run, where their nodes are")
{
    Fixture fixture(false);
    Node3D& wall = fixture.box("Wall", {2.0f, 0.0f, 0.0f});
    const DebugChannels colliders{std::string(kCollidersDebugChannel)};
    DebugLines drawn = fixture.lines(colliders);
    CHECK(drawn.tested().size() == 24);
    CHECK(hasVertexNear(drawn.tested(), Vec3(2.5f, 0.5f, 0.5f)));

    wall.transform().setPosition({5.0f, 0.0f, 0.0f});
    drawn = fixture.lines(colliders);
    CHECK(hasVertexNear(drawn.tested(), Vec3(5.5f, 0.5f, 0.5f)));
    // Триггеров нет - канал триггеров пуст.
    CHECK(fixture.lines({std::string(kTriggersDebugChannel)}).empty());
}

TEST_CASE_FIXTURE(Fixture, "DebugDraw: velocities and sleeping bodies have their channels")
{
    RigidBody& body = ball("Ball", {0.0f, 10.0f, 0.0f});
    body.setGravityScale(0.0f);
    scene.advance(kFrame);
    body.setVelocity({4.0f, 0.0f, 0.0f});
    scene.advance(kFrame);
    CHECK_FALSE(lines({std::string(kVelocitiesDebugChannel)}).empty());

    const DebugChannels sleep{std::string(kSleepDebugChannel)};
    CHECK(lines(sleep).empty());
    body.sleep();
    CHECK_FALSE(lines(sleep).empty());
}

TEST_CASE_FIXTURE(Fixture, "DebugDraw: frame data has lines only when the frame asks for debug")
{
    debug().setViewerChannels(kGame);
    scene.advance(kFrame);
    debug().line(Vec3(0.0f), Vec3(1.0f), {.color = Vec4(1.0f, 0.5f, 0.0f, 1.0f), .depthTest = false});
    scene.updateTransforms();

    FrameData plain = extractFrame(scene, kAspect);
    CHECK(plain.debugLines.empty());
    extractDebugLines(scene, {}, plain);
    CHECK(plain.debugLines.empty());

    FrameData debugged = extractFrame(scene, kAspect);
    extractDebugLines(scene, kGame, debugged);
    REQUIRE(debugged.debugLines.overlay.size() == 2);
    CHECK(debugged.debugLines.tested.empty());
    // Цвет - уже в линейном RGB.
    CHECK(debugged.debugLines.overlay.front().color.g == doctest::Approx(math::srgbToLinear(0.5f)));
}
