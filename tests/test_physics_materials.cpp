#include "EngineRegistry.hpp"
#include "TestFiles.hpp"

#include <doctest/doctest.h>

#include <yqlora/core/EngineServices.hpp>
#include <yqlora/core/JsonFile.hpp>
#include <yqlora/input/Input.hpp>
#include <yqlora/physics/BoxCollider.hpp>
#include <yqlora/physics/PhysicsMaterialData.hpp>
#include <yqlora/physics/PhysicsMaterialFile.hpp>
#include <yqlora/physics/PhysicsSettings.hpp>
#include <yqlora/physics/RigidBody.hpp>
#include <yqlora/physics/SphereCollider.hpp>
#include <yqlora/project/Project.hpp>
#include <yqlora/resource/BuiltinResources.hpp>
#include <yqlora/resource/ResourceCache.hpp>
#include <yqlora/scene/Node3D.hpp>
#include <yqlora/scene/Scene.hpp>
#include <yqlora/scene/Transform3D.hpp>

#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <optional>
#include <string>

using namespace yqlora;

namespace {

constexpr float kFrame = 1.0f / 60.0f;
constexpr int kFramesPerSecond = 60;
// Склон, на котором держит трение больше tan(25°) = 0.47: материал по
// умолчанию (0.6) держит, лёд - нет.
constexpr float kSlopeDegrees = 25.0f;
constexpr PhysicsMaterialData kIce{.friction = 0.05f};
constexpr PhysicsMaterialData kRubber{.friction = 1.0f};
constexpr float kBounce = 0.8f;
constexpr PhysicsMaterialData kBouncy{.restitution = kBounce};

struct Fixture {
    Fixture()
    {
        REQUIRE(writePhysicsMaterialFile(directory / "ice.yphysmat", kIce));
        REQUIRE(writePhysicsMaterialFile(directory / "rubber.yphysmat", kRubber));
        REQUIRE(writePhysicsMaterialFile(directory / "bouncy.yphysmat", kBouncy));
        REQUIRE(Project::create(directory, "Game", registry));
        project.emplace(*Project::open(directory, registry));
        services.setProject(&*project);
        scene.setPlaying(true);
        scene.createRoot<Node3D>("Root");
    }

    ~Fixture()
    {
        services.setProject(nullptr);
    }

    ResourceId material(const std::string& file) const
    {
        const ResourceId id = project->resources().id(file);
        REQUIRE(id != 0);

        return id;
    }

    // Склон шириной 4 м вдоль X с ящиком на нём, у обоих материал surface
    // (0 - по умолчанию). Возвращает ящик.
    Node3D& boxOnSlope(float z, ResourceId surface)
    {
        const Quat tilt(glm::radians(Vec3(0.0f, 0.0f, kSlopeDegrees)));
        auto& slope = scene.root()->addChild<Node3D>("Slope");
        slope.transform().setPosition({0.0f, 0.0f, z});
        slope.transform().setRotation(tilt);
        auto& slopeCollider = slope.addComponent<BoxCollider>();
        slopeCollider.setSize({40.0f, 1.0f, 4.0f});
        slopeCollider.setMaterial({surface});

        auto& box = scene.root()->addChild<Node3D>("Box");
        box.transform().setPosition(Vec3(0.0f, 0.0f, z) + tilt * Vec3(0.0f, 1.01f, 0.0f));
        box.transform().setRotation(tilt);
        box.addComponent<RigidBody>().setInterpolation(false);
        box.addComponent<BoxCollider>().setMaterial({surface});

        return box;
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
    std::filesystem::path directory = test::freshDirectory("physics materials");
    std::optional<Project> project;
    Scene scene{services};
};

float travelled(const Node3D& node, const Vec3& start)
{
    return glm::length(node.worldPosition() - start);
}

} // namespace

TEST_CASE("PhysicsMaterial: the elder mode of two wins in either order, values combine by it")
{
    CHECK(combineMode(PhysicsCombine::Average, PhysicsCombine::Maximum) == PhysicsCombine::Maximum);
    CHECK(combineMode(PhysicsCombine::Maximum, PhysicsCombine::Average) == PhysicsCombine::Maximum);
    CHECK(combineMode(PhysicsCombine::Minimum, PhysicsCombine::Multiply) == PhysicsCombine::Multiply);
    CHECK(combineMode(PhysicsCombine::Average, PhysicsCombine::Minimum) == PhysicsCombine::Minimum);

    CHECK(combineValues(0.2f, 0.6f, PhysicsCombine::Average) == doctest::Approx(0.4f));
    CHECK(combineValues(0.2f, 0.6f, PhysicsCombine::Minimum) == doctest::Approx(0.2f));
    CHECK(combineValues(0.2f, 0.6f, PhysicsCombine::Multiply) == doctest::Approx(0.12f));
    CHECK(combineValues(0.2f, 0.6f, PhysicsCombine::Maximum) == doctest::Approx(0.6f));

    const PhysicsMaterialData ice{.friction = 0.05f, .frictionCombine = PhysicsCombine::Minimum};
    const PhysicsMaterialData rubber{.friction = 1.0f, .restitution = 0.5f};
    CHECK(combinedFriction(ice, rubber) == doctest::Approx(0.05f));
    CHECK(combinedFriction(rubber, ice) == doctest::Approx(0.05f));
    CHECK(combinedRestitution(ice, rubber) == doctest::Approx(0.25f));
}

TEST_CASE_FIXTURE(Fixture, "PhysicsMaterial: a .yphysmat is a resource, broken values are refused or held in range")
{
    test::writeFile(directory / "glow.yphysmat", R"({"version": 1, "frictionCombine": "Glow"})");
    test::writeFile(directory / "wild.yphysmat", R"({"version": 1, "friction": -2, "restitution": 3})");
    const ResourceId glow = project->resources().add("glow.yphysmat");
    const ResourceId wild = project->resources().add("wild.yphysmat");
    ResourceCache& cache = services.resources();

    CHECK(cache.kindOf("ice.yphysmat") == PhysicsMaterialData::kResourceKind);
    const auto* ice = cache.find<PhysicsMaterialData>(material("ice.yphysmat"));
    REQUIRE(ice);
    CHECK(*ice == kIce);
    CHECK_FALSE(cache.find<PhysicsMaterialData>(glow));
    const auto* held = cache.find<PhysicsMaterialData>(wild);
    REQUIRE(held);
    CHECK(held->friction == 0.0f);
    CHECK(held->restitution == kMaxRestitution);

    const auto* builtin = cache.find<PhysicsMaterialData>(builtin::kDefaultPhysicsMaterial);
    REQUIRE(builtin);
    CHECK(*builtin == PhysicsMaterialData{});
}

TEST_CASE_FIXTURE(Fixture, "PhysicsMaterial: a box slides down an icy slope and holds on a rubber one")
{
    Node3D& onIce = boxOnSlope(0.0f, material("ice.yphysmat"));
    Node3D& onRubber = boxOnSlope(10.0f, material("rubber.yphysmat"));
    const Vec3 iceStart = onIce.worldPosition();
    const Vec3 rubberStart = onRubber.worldPosition();
    run(2.0f);
    CHECK(travelled(onIce, iceStart) > 3.0f);
    CHECK(travelled(onRubber, rubberStart) < 0.05f);
}

TEST_CASE_FIXTURE(Fixture, "PhysicsMaterial: a ball bounces back to the height its restitution gives")
{
    constexpr float kRadius = 0.5f;
    constexpr float kDrop = 2.0f;
    const ResourceId bouncy = material("bouncy.yphysmat");
    auto& ground = scene.root()->addChild<Node3D>("Ground");
    ground.transform().setPosition({0.0f, -0.5f, 0.0f});
    auto& floor = ground.addComponent<BoxCollider>();
    floor.setSize({40.0f, 1.0f, 40.0f});
    floor.setMaterial({bouncy});
    auto& ball = scene.root()->addChild<Node3D>("Ball");
    ball.transform().setPosition({0.0f, kRadius + kDrop, 0.0f});
    ball.addComponent<RigidBody>().setInterpolation(false);
    ball.addComponent<SphereCollider>().setMaterial({bouncy});

    // Вниз до удара, затем вверх до самой высокой точки.
    float previous = ball.worldPosition().y;
    bool rising = false;
    float peak = 0.0f;
    for (int i = 0; i < 3 * kFramesPerSecond; ++i) {
        scene.advance(kFrame);
        const float height = ball.worldPosition().y;
        if (!rising && height > previous) {
            rising = true;
        } else if (rising && height < previous) {
            break;
        }
        peak = std::max(peak, rising ? height : 0.0f);
        previous = height;
    }
    REQUIRE(rising);
    // Скорость удара теряет долю e: высота - e^2 от высоты падения.
    CHECK(peak - kRadius == doctest::Approx(kBounce * kBounce * kDrop).epsilon(0.1));
}

TEST_CASE_FIXTURE(Fixture, "PhysicsMaterial: colliders without a material take the project default, changed in Play")
{
    Node3D& box = boxOnSlope(0.0f, 0);
    const Vec3 start = box.worldPosition();
    run(1.0f);
    CHECK(travelled(box, start) < 0.05f);

    project->settings().get<PhysicsSettings>().setDefaultMaterial({material("ice.yphysmat")});
    run(1.0f);
    CHECK(travelled(box, start) > 1.0f);
}

TEST_CASE_FIXTURE(Fixture, "PhysicsMaterial: an edited material acts from the next step without rebuilding bodies")
{
    const ResourceId rubber = material("rubber.yphysmat");
    Node3D& box = boxOnSlope(0.0f, rubber);
    const Vec3 start = box.worldPosition();
    run(1.0f);
    CHECK(travelled(box, start) < 0.05f);

    REQUIRE(services.resources().replace(rubber, kIce));
    run(1.0f);
    CHECK(travelled(box, start) > 1.0f);
}
