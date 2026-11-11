#include "EngineRegistry.hpp"
#include "TestFiles.hpp"

#include <doctest/doctest.h>

#include <yqlora/core/EngineServices.hpp>
#include <yqlora/editor/ProjectSetup.hpp>
#include <yqlora/input/Input.hpp>
#include <yqlora/physics/JoltSetup.hpp>
#include <yqlora/physics/LayerCollisionMatrix.hpp>
#include <yqlora/physics/PhysicsLayers.hpp>
#include <yqlora/physics/PhysicsRuntime.hpp>
#include <yqlora/physics/PhysicsSettings.hpp>
#include <yqlora/physics/PhysicsWorld.hpp>
#include <yqlora/physics/PhysicsWorldAccess.hpp>
#include <yqlora/project/Project.hpp>
#include <yqlora/scene/Node3D.hpp>
#include <yqlora/scene/Scene.hpp>
#include <yqlora/scene/SceneSerializer.hpp>

#include <memory>
#include <string>
#include <vector>

using namespace yqlora;

namespace {

constexpr int kWalls = 3;
constexpr int kPlayers = 7;
constexpr float kFrame = 0.1f;

struct Fixture {
    test::EngineRegistry registry;
    Input input;
    EngineServices services{input, registry};
};

} // namespace

TEST_CASE("Physics layers: a node layer and the motion make the object layer and its broad phase")
{
    for (const int layer : {0, kPlayers, kLayerCount - 1}) {
        for (const bool moving : {false, true}) {
            const PhysicsObjectLayer object = physicsObjectLayer(layer, moving);
            CHECK(object < kPhysicsObjectLayerCount);
            CHECK(nodeLayerOf(object) == layer);
            CHECK(isMovingLayer(object) == moving);
            CHECK(broadPhaseOf(object) == (moving ? PhysicsBroadPhase::Moving : PhysicsBroadPhase::Static));
        }
    }
}

TEST_CASE("Physics layers: static never meets static, the matrix decides the rest in both directions")
{
    LayerCollisionMatrix matrix;
    const auto wall = physicsObjectLayer(kWalls, false);
    const auto otherWall = physicsObjectLayer(kWalls, false);
    const auto player = physicsObjectLayer(kPlayers, true);
    const auto crate = physicsObjectLayer(kWalls, true);

    CHECK_FALSE(physicsLayersCollide(matrix, wall, otherWall));
    CHECK(physicsLayersCollide(matrix, wall, player));
    CHECK(physicsLayersCollide(matrix, player, crate));
    CHECK_FALSE(physicsLayerMeetsBroadPhase(wall, PhysicsBroadPhase::Static));
    CHECK(physicsLayerMeetsBroadPhase(wall, PhysicsBroadPhase::Moving));
    CHECK(physicsLayerMeetsBroadPhase(player, PhysicsBroadPhase::Static));

    matrix.setCollides(kPlayers, kWalls, false);
    CHECK_FALSE(physicsLayersCollide(matrix, wall, player));
    CHECK_FALSE(physicsLayersCollide(matrix, player, wall));
    CHECK_FALSE(physicsLayersCollide(matrix, player, crate));
    // Слои игрока между собой по-прежнему сталкиваются.
    CHECK(physicsLayersCollide(matrix, player, player));
}

TEST_CASE("LayerCollisionMatrix: symmetric whatever is written, stored as the pairs that do not collide")
{
    LayerCollisionMatrix matrix;
    CHECK(matrix.ignoredPairs().empty());
    CHECK(matrix.mask(0) == kAllLayers);

    matrix.setCollides(kPlayers, kWalls, false);
    CHECK_FALSE(matrix.collides(kWalls, kPlayers));
    CHECK(matrix.ignoredPairs() == std::vector<LayerPair>{{kWalls, kPlayers}});

    // Обратный порядок, повтор и номера вне слоёв: остаётся одна пара.
    matrix.setIgnoredPairs({{kPlayers, kWalls}, {kWalls, kPlayers}, {-1, kWalls}, {kLayerCount, 0}});
    CHECK(matrix.ignoredPairs() == std::vector<LayerPair>{{kWalls, kPlayers}});
    CHECK_FALSE(matrix.collides(-1, kWalls));
    CHECK(matrix.mask(kLayerCount) == 0);

    matrix.setCollides(kWalls, kPlayers, true);
    CHECK(matrix.ignoredPairs().empty());
}

TEST_CASE_FIXTURE(Fixture, "PhysicsWorld: a scene without bodies creates no Jolt world")
{
    Scene scene{services};
    scene.setPlaying(true);
    scene.createRoot<Node3D>("Root");
    const int holders = JoltSetup::holders();
    for (int frame = 0; frame < 3; ++frame) {
        scene.advance(kFrame);
    }
    const PhysicsWorld* world = scene.system<PhysicsWorld>();
    REQUIRE(world);
    CHECK_FALSE(world->simulating());
    CHECK(world->stepCount() == 0);
    CHECK_FALSE(services.physics().started());
    CHECK(JoltSetup::holders() == holders);
}

TEST_CASE_FIXTURE(Fixture, "PhysicsWorld: a Jolt world steps once per fixed step, gravity from the settings and code")
{
    const int holders = JoltSetup::holders();
    {
        Scene scene{services};
        scene.createRoot<Node3D>("Root");
        auto& world = *scene.system<PhysicsWorld>();
        PhysicsWorldAccess::ensureWorld(world);
        CHECK(world.simulating());
        CHECK(JoltSetup::holders() == holders + 1);
        CHECK(world.gravity() == kDefaultGravity);

        world.setGravity({0.0f, -1.0f, 0.0f});
        CHECK(world.gravity() == Vec3{0.0f, -1.0f, 0.0f});

        // Кадр в два фиксированных шага: два шага мира.
        const float fixedStep = scene.time().fixedStep();
        scene.advance(2.0f * fixedStep);
        CHECK(world.stepCount() == 2);
    }
    // Мир ушёл, а среда служб ещё держит Jolt; уходит вместе со службами.
    CHECK(JoltSetup::holders() == holders + 1);
}

TEST_CASE("PhysicsRuntime: the global setup of Jolt is held by started runtimes only")
{
    const int holders = JoltSetup::holders();
    {
        PhysicsRuntime first;
        PhysicsRuntime second;
        CHECK(JoltSetup::holders() == holders);
        first.start();
        second.start();
        CHECK(JoltSetup::holders() == holders + 2);
    }
    CHECK(JoltSetup::holders() == holders);
}

TEST_CASE("PhysicsSettings: gravity, iterations and the collision matrix survive save and load")
{
    test::EngineRegistry registry;
    const SceneSerializer serializer(registry);
    std::string error;
    const auto created = editor::createProject(test::freshDirectory("physics settings"), "Game", serializer, error);
    REQUIRE(created);
    const auto directory = created->directory();
    {
        Project project = *Project::open(directory, registry);
        auto& physics = project.settings().get<PhysicsSettings>();
        CHECK(physics.gravity() == kDefaultGravity);
        physics.setGravity({0.0f, -3.7f, 0.0f});
        physics.setVelocitySteps(kMaxSolverSteps + 1);
        physics.setPositionSteps(4);
        physics.collisions().setCollides(kPlayers, kWalls, false);
        REQUIRE(project.settings().save(physics));
    }
    const Project reopened = *Project::open(directory, registry);
    const auto& physics = reopened.settings().get<PhysicsSettings>();
    CHECK(physics.gravity() == Vec3{0.0f, -3.7f, 0.0f});
    CHECK(physics.velocitySteps() == kMaxSolverSteps);
    CHECK(physics.positionSteps() == 4);
    CHECK_FALSE(physics.collisions().collides(kWalls, kPlayers));
    CHECK(physics.collisions().ignoredPairs().size() == 1);
}
