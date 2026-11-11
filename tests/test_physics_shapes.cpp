#include "EngineRegistry.hpp"
#include "TestFiles.hpp"

#include <doctest/doctest.h>

#include <yqlora/core/EngineServices.hpp>
#include <yqlora/core/LogSubscription.hpp>
#include <yqlora/input/Input.hpp>
#include <yqlora/physics/BoxCollider.hpp>
#include <yqlora/physics/ColliderShape.hpp>
#include <yqlora/physics/MeshCollider.hpp>
#include <yqlora/physics/PhysicsWorld.hpp>
#include <yqlora/physics/PhysicsWorldAccess.hpp>
#include <yqlora/physics/RigidBody.hpp>
#include <yqlora/physics/SphereCollider.hpp>
#include <yqlora/project/Project.hpp>
#include <yqlora/render/MeshData.hpp>
#include <yqlora/resource/BuiltinResources.hpp>
#include <yqlora/resource/ResourceCache.hpp>
#include <yqlora/resource/ResourceLoader.hpp>
#include <yqlora/scene/Node3D.hpp>
#include <yqlora/scene/Scene.hpp>
#include <yqlora/scene/Transform3D.hpp>

#include <array>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <variant>

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

    Node3D& ground(float thickness = 1.0f)
    {
        auto& node = scene.root()->addChild<Node3D>("Ground");
        node.transform().setPosition({0.0f, -thickness * 0.5f, 0.0f});
        node.addComponent<BoxCollider>().setSize({40.0f, thickness, 40.0f});

        return node;
    }

    Node3D& body(const std::string& name, Vec3 position)
    {
        auto& node = scene.root()->addChild<Node3D>(name);
        node.transform().setPosition(position);
        auto& rigid = node.addComponent<RigidBody>();
        rigid.setInterpolation(false);
        rigid.setLinearDamping(0.0f);

        return node;
    }

    void run(float seconds)
    {
        for (int i = 0; i < static_cast<int>(seconds * kFramesPerSecond); ++i) {
            scene.advance(kFrame);
        }
    }

    PhysicsWorld& world()
    {
        return *scene.system<PhysicsWorld>();
    }

    test::EngineRegistry registry;
    Input input;
    EngineServices services{input, registry};
    Scene scene{services};
};

float heightOf(const Node3D& node)
{
    return node.worldPosition().y;
}

// Файл .tri - треугольник высотой в длину его текста: правка файла меняет меш.
class TriangleLoader final : public ResourceLoader {
public:
    std::span<const std::string_view> extensions() const override
    {
        return kExtensions;
    }

    std::string_view kind() const override
    {
        return MeshData::kResourceKind;
    }

    LoadResult load(const LoadRequest& request) const override
    {
        std::ifstream stream(request.file);
        const std::string content{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
        MeshData mesh;
        const Vec3 normal(0.0f, 0.0f, 1.0f);
        mesh.vertices = {{Vec3(0.0f), normal}, {Vec3(1.0f, 0.0f, 0.0f), normal}};
        mesh.vertices.push_back({Vec3(0.0f, static_cast<float>(content.size()), 0.0f), normal});
        mesh.indices = {0, 1, 2};
        finishSingleMaterial(mesh);
        LoadResult result;
        result.resources.push_back(makeLoadedResource(std::string(), std::move(mesh)));

        return result;
    }

private:
    static constexpr std::array<std::string_view, 1> kExtensions{".tri"};
};

} // namespace

TEST_CASE("scaledShape: sizes follow the scale, round shapes take the largest axis and say so")
{
    bool lost = false;
    const auto box = std::get<BoxShape>(scaledShape(BoxShape{Vec3(0.5f)}, {1.0f, 2.0f, 3.0f}, lost));
    CHECK(box.halfExtents == Vec3(0.5f, 1.0f, 1.5f));
    CHECK_FALSE(lost);

    const auto sphere = std::get<SphereShape>(scaledShape(SphereShape{0.5f}, {1.0f, 3.0f, 1.0f}, lost));
    CHECK(sphere.radius == doctest::Approx(1.5f));
    CHECK(lost);

    // Капсула, вытянутая по Y, остаётся капсулой: только выше.
    const auto tall = std::get<CapsuleShape>(scaledShape(CapsuleShape{0.5f, 0.5f}, {1.0f, 2.0f, 1.0f}, lost));
    CHECK(tall.radius == doctest::Approx(0.5f));
    CHECK(tall.halfHeight + tall.radius == doctest::Approx(2.0f));
    CHECK_FALSE(lost);
    scaledShape(CapsuleShape{0.5f, 0.5f}, {1.0f, 1.0f, 2.0f}, lost);
    CHECK(lost);

    const auto cylinder = std::get<CylinderShape>(scaledShape(CylinderShape{0.5f, 1.0f}, Vec3(2.0f), lost));
    CHECK(cylinder.radius == doctest::Approx(1.0f));
    CHECK(cylinder.halfHeight == doctest::Approx(2.0f));
    CHECK_FALSE(lost);
}

TEST_CASE_FIXTURE(Fixture, "Physics scale: a box collider on a node scaled non-uniformly rests on its scaled size")
{
    ground();
    auto& tall = body("Tall", {0.0f, 3.0f, 0.0f});
    tall.transform().setScale({1.0f, 2.0f, 1.0f});
    tall.addComponent<BoxCollider>();
    run(3.0f);
    CHECK(heightOf(tall) == doctest::Approx(1.0f).epsilon(0.03));
}

TEST_CASE_FIXTURE(Fixture, "Physics scale: a sphere under non-uniform scale takes the largest axis and says so once")
{
    int warnings = 0;
    const log::Subscription subscription = log::subscribe([&](log::Level level, std::string_view message) {
        if (level == log::Level::Warning && message.find("non-uniform") != std::string_view::npos) {
            ++warnings;
        }
    });
    ground();
    auto& ball = body("Ball", {0.0f, 3.0f, 0.0f});
    ball.transform().setScale({1.0f, 3.0f, 1.0f});
    auto& sphere = ball.addComponent<SphereCollider>();
    PhysicsWorldAccess::flush(world());
    // Каждая правка перестраивает тело, но сообщение одно.
    sphere.setRadius(0.5f);
    PhysicsWorldAccess::flush(world());
    run(3.0f);
    CHECK(warnings == 1);
    CHECK(heightOf(ball) == doctest::Approx(1.5f).epsilon(0.03));

    // Масштаб узла тела поменяли: форма следует.
    ball.transform().setScale(Vec3(1.0f));
    run(2.0f);
    CHECK(heightOf(ball) == doctest::Approx(0.5f).epsilon(0.05));
}

TEST_CASE_FIXTURE(Fixture, "Physics meshes: a mesh shape is made once for a hundred colliders")
{
    constexpr int kColliders = 100;
    for (int i = 0; i < kColliders; ++i) {
        auto& node = scene.root()->addChild<Node3D>("Wall " + std::to_string(i));
        node.transform().setPosition({static_cast<float>(i) * 2.0f, 0.0f, 0.0f});
        node.addComponent<MeshCollider>().setMesh({builtin::kCubeMesh});
    }
    PhysicsWorldAccess::flush(world());
    CHECK(PhysicsWorldAccess::bodyCount(world()) == kColliders);
    CHECK(PhysicsWorldAccess::meshShapesCooked(world()) == 1);
}

TEST_CASE_FIXTURE(Fixture, "Physics meshes: a box falls onto a floor made of mesh triangles and stops")
{
    auto& floor = scene.root()->addChild<Node3D>("Floor");
    floor.transform().setScale({20.0f, 1.0f, 20.0f});
    floor.addComponent<MeshCollider>().setMesh({builtin::kPlaneMesh});
    auto& box = body("Box", {0.0f, 3.0f, 0.0f});
    box.addComponent<BoxCollider>();
    run(3.0f);
    CHECK(heightOf(box) == doctest::Approx(0.5f).epsilon(0.05));
}

TEST_CASE_FIXTURE(Fixture, "Physics meshes: a dynamic body uses the convex hull of a mesh collider and says so once")
{
    int warnings = 0;
    const log::Subscription subscription = log::subscribe([&](log::Level level, std::string_view message) {
        if (level == log::Level::Warning && message.find("convex hull") != std::string_view::npos) {
            ++warnings;
        }
    });
    ground();
    auto& rock = body("Rock", {0.0f, 3.0f, 0.0f});
    rock.addComponent<MeshCollider>().setMesh({builtin::kCubeMesh});
    run(3.0f);
    CHECK(warnings == 1);
    CHECK(heightOf(rock) == doctest::Approx(0.5f).epsilon(0.05));
}

TEST_CASE_FIXTURE(Fixture, "RigidBody: continuous collision stops a fast small ball at a thin floor")
{
    constexpr float kThin = 0.02f;
    ground(kThin);
    auto& fast = body("Fast", {0.0f, 120.0f, 0.0f});
    fast.addComponent<SphereCollider>().setRadius(0.05f);
    fast.getComponent<RigidBody>()->setContinuousCollision(true);
    auto& plain = body("Plain", {3.0f, 120.0f, 0.0f});
    plain.addComponent<SphereCollider>().setRadius(0.05f);
    run(6.0f);
    CHECK(heightOf(fast) > 0.0f);
    // Без него шаг больше пола и шара вместе: шар проходит насквозь.
    CHECK(heightOf(plain) < -1.0f);
}

TEST_CASE_FIXTURE(Fixture, "RigidBody: a locked axis does not move, every axis locked makes the body static")
{
    auto& hanging = body("Hanging", {0.0f, 5.0f, 0.0f});
    hanging.addComponent<BoxCollider>();
    hanging.getComponent<RigidBody>()->setLockPosition({.x = false, .y = true, .z = false});
    run(1.0f);
    CHECK(heightOf(hanging) == doctest::Approx(5.0f));

    int warnings = 0;
    const log::Subscription subscription = log::subscribe([&](log::Level level, std::string_view message) {
        if (level == log::Level::Warning && message.find("every axis locked") != std::string_view::npos) {
            ++warnings;
        }
    });
    auto& frozen = body("Frozen", {3.0f, 5.0f, 0.0f});
    frozen.addComponent<BoxCollider>();
    auto& rigid = *frozen.getComponent<RigidBody>();
    rigid.setLockPosition({.x = true, .y = true, .z = true});
    rigid.setLockRotation({.x = true, .y = true, .z = true});
    run(1.0f);
    CHECK(heightOf(frozen) == doctest::Approx(5.0f));
    CHECK(warnings == 1);
}

TEST_CASE_FIXTURE(Fixture, "RigidBody: a body that starts asleep hangs until something hits it")
{
    auto& sleeper = body("Sleeper", {0.0f, 5.0f, 0.0f});
    sleeper.addComponent<BoxCollider>();
    sleeper.getComponent<RigidBody>()->setStartAsleep(true);
    run(1.0f);
    CHECK(heightOf(sleeper) == doctest::Approx(5.0f));

    auto& hammer = body("Hammer", {0.0f, 8.0f, 0.0f});
    hammer.addComponent<BoxCollider>();
    run(2.0f);
    CHECK(heightOf(sleeper) < 4.0f);
}

TEST_CASE_FIXTURE(Fixture, "Physics meshes: a changed mesh is made again and its bodies follow")
{
    const auto directory = test::freshDirectory("physics mesh reload");
    test::writeFile(directory / "wall.tri", "ab");
    REQUIRE(Project::create(directory, "Game", registry));
    std::optional<Project> project(*Project::open(directory, registry));
    services.resources().addLoader(std::make_unique<TriangleLoader>());
    services.setProject(&*project);
    const ResourceId wall = project->resources().id("wall.tri");
    REQUIRE(wall != 0);

    for (int i = 0; i < 2; ++i) {
        scene.root()->addChild<Node3D>("Wall " + std::to_string(i)).addComponent<MeshCollider>().setMesh({wall});
    }
    PhysicsWorldAccess::flush(world());
    CHECK(PhysicsWorldAccess::meshShapesCooked(world()) == 1);
    PhysicsWorldAccess::flush(world());
    CHECK(PhysicsWorldAccess::meshShapesCooked(world()) == 1);

    test::writeFile(directory / "wall.tri", "abcd");
    REQUIRE(services.resources().reload(wall));
    PhysicsWorldAccess::flush(world());
    CHECK(PhysicsWorldAccess::meshShapesCooked(world()) == 2);
    CHECK(PhysicsWorldAccess::bodyCount(world()) == 2);
    services.setProject(nullptr);
}
