#include "EngineRegistry.hpp"
#include "TestFiles.hpp"

#include <doctest/doctest.h>

#include <yqlora/core/EngineServices.hpp>
#include <yqlora/editor/ProjectSetup.hpp>
#include <yqlora/input/Input.hpp>
#include <yqlora/physics/BoxCollider.hpp>
#include <yqlora/physics/PhysicsSettings.hpp>
#include <yqlora/physics/PhysicsWorld.hpp>
#include <yqlora/physics/PhysicsWorldAccess.hpp>
#include <yqlora/physics/RigidBody.hpp>
#include <yqlora/physics/SphereCollider.hpp>
#include <yqlora/project/Project.hpp>
#include <yqlora/scene/Node3D.hpp>
#include <yqlora/scene/Scene.hpp>
#include <yqlora/scene/SceneSerializer.hpp>
#include <yqlora/scene/TimeSettings.hpp>
#include <yqlora/scene/Transform3D.hpp>

#include <optional>
#include <string>

using namespace yqlora;

namespace {

constexpr float kFrame = 1.0f / 60.0f;
constexpr int kFramesPerSecond = 60;
constexpr int kGroundLayer = 3;
constexpr int kCrateLayer = 7;

// Переносит свой узел в точку target в первом же фиксированном шаге: так
// скрипт двигает узел с телом из onFixedUpdate.
struct Teleporter final : Component {
    YQLORA_TYPE(Teleporter, Component)

    void onFixedUpdate(float /*stepSeconds*/) override
    {
        if (!done) {
            static_cast<Node3D*>(owner())->transform().setPosition(target);
            done = true;
        }
    }

    Vec3 target{0.0f};
    bool done = false;
};

struct RegistryWithTeleporter : test::EngineRegistry {
    RegistryWithTeleporter()
    {
        registerComponent<Teleporter>();
    }
};

struct Fixture {
    Fixture()
    {
        scene.setPlaying(true);
        scene.createRoot<Node3D>("Root");
    }

    // Динамическое тело с коллайдером-ящиком на высоте y.
    Node3D& crate(const std::string& name, Vec3 position, bool interpolation = false)
    {
        auto& node = scene.root()->addChild<Node3D>(name);
        node.transform().setPosition(position);
        auto& body = node.addComponent<RigidBody>();
        body.setLinearDamping(0.0f);
        body.setInterpolation(interpolation);
        node.addComponent<BoxCollider>();

        return node;
    }

    // Пол - коллайдер без тела: верх на y = 0.
    Node3D& ground(int layer = 0)
    {
        auto& node = scene.root()->addChild<Node3D>("Ground");
        node.transform().setPosition({0.0f, -0.5f, 0.0f});
        node.setLayer(layer);
        node.addComponent<BoxCollider>().setSize({40.0f, 1.0f, 40.0f});

        return node;
    }

    void run(float seconds)
    {
        const int frames = static_cast<int>(seconds * kFramesPerSecond);
        for (int i = 0; i < frames; ++i) {
            scene.advance(kFrame);
        }
    }

    PhysicsWorld& world()
    {
        return *scene.system<PhysicsWorld>();
    }

    RegistryWithTeleporter registry;
    Input input;
    EngineServices services{input, registry};
    Scene scene{services};
};

// Проект для настроек: матрица столкновений, фиксированный шаг.
struct ProjectFixture : Fixture {
    ProjectFixture()
    {
        const SceneSerializer serializer(registry);
        std::string error;
        project = editor::createProject(test::freshDirectory("physics bodies"), "Game", serializer, error);
        REQUIRE(project);
        services.setProject(&*project);
    }

    ~ProjectFixture()
    {
        services.setProject(nullptr);
    }

    ProjectFixture(const ProjectFixture&) = delete;
    ProjectFixture& operator=(const ProjectFixture&) = delete;

    std::optional<Project> project;
};

float heightOf(const Node3D& node)
{
    return node.worldPosition().y;
}

} // namespace

TEST_CASE_FIXTURE(Fixture, "RigidBody: a box falls 4.9 m in a second under gravity")
{
    const Node3D& box = crate("Box", {0.0f, 10.0f, 0.0f});
    run(1.0f);
    // Шаги дискретны: ошибка интегрирования - доли дециметра.
    CHECK(heightOf(box) == doctest::Approx(10.0f - 0.5f * kEarthGravity).epsilon(0.02));
}

TEST_CASE_FIXTURE(Fixture, "RigidBody: a sphere rests on the ground made by a collider without a body")
{
    ground();
    auto& ball = scene.root()->addChild<Node3D>("Ball");
    ball.transform().setPosition({0.0f, 2.0f, 0.0f});
    ball.addComponent<RigidBody>().setInterpolation(false);
    ball.addComponent<SphereCollider>();
    run(3.0f);
    CHECK(PhysicsWorldAccess::bodyCount(world()) == 2);
    CHECK(heightOf(ball) == doctest::Approx(0.5f).epsilon(0.05));
}

TEST_CASE_FIXTURE(Fixture, "RigidBody: colliders on children make one body with the mass of the body")
{
    auto& robot = scene.root()->addChild<Node3D>("Robot");
    robot.addComponent<RigidBody>().setMass(5.0f);
    auto& left = robot.addChild<Node3D>("Left");
    left.transform().setPosition({-1.0f, 0.0f, 0.0f});
    left.addComponent<BoxCollider>();
    auto& right = robot.addChild<Node3D>("Right");
    right.transform().setPosition({1.0f, 0.0f, 0.0f});
    right.addComponent<SphereCollider>();
    PhysicsWorldAccess::flush(world());
    CHECK(PhysicsWorldAccess::bodyCount(world()) == 1);
    CHECK(PhysicsWorldAccess::bodyMass(world(), robot.id()) == doctest::Approx(5.0f));
}

TEST_CASE_FIXTURE(Fixture, "RigidBody: adding, disabling and moving colliders and bodies changes the bodies")
{
    auto& robot = scene.root()->addChild<Node3D>("Robot");
    auto& rigid = robot.addComponent<RigidBody>();
    robot.addChild<Node3D>("Arm").addComponent<BoxCollider>();
    auto& rock = scene.root()->addChild<Node3D>("Rock");
    auto& rockCollider = rock.addComponent<BoxCollider>();
    PhysicsWorldAccess::flush(world());
    CHECK(PhysicsWorldAccess::bodyCount(world()) == 2);

    // Камень под роботом - часть его тела.
    scene.setParent(rock, robot, KeepWorld::Yes);
    PhysicsWorldAccess::flush(world());
    CHECK(PhysicsWorldAccess::bodyCount(world()) == 1);

    // Без RigidBody коллайдеры - каждый своё неподвижное тело.
    rigid.setEnabled(false);
    PhysicsWorldAccess::flush(world());
    CHECK(PhysicsWorldAccess::bodyCount(world()) == 2);

    rockCollider.setEnabled(false);
    PhysicsWorldAccess::flush(world());
    CHECK(PhysicsWorldAccess::bodyCount(world()) == 1);

    rigid.setEnabled(true);
    robot.setActive(false);
    PhysicsWorldAccess::flush(world());
    CHECK(PhysicsWorldAccess::bodyCount(world()) == 0);
}

TEST_CASE_FIXTURE(Fixture, "RigidBody: a child collider moved relative to its body changes the shape")
{
    ground();
    auto& body = scene.root()->addChild<Node3D>("Body");
    body.transform().setPosition({0.0f, 2.0f, 0.0f});
    body.addComponent<RigidBody>().setInterpolation(false);
    auto& foot = body.addChild<Node3D>("Foot");
    foot.addComponent<BoxCollider>();
    run(2.0f);
    // Коллайдер у самого узла: ящик стоит, узел на полметра над полом.
    CHECK(heightOf(body) == doctest::Approx(0.5f).epsilon(0.05));

    // Ступню подняли на метр выше узла: узел теперь на полметра ниже пола.
    foot.transform().setPosition({0.0f, 1.0f, 0.0f});
    run(2.0f);
    CHECK(heightOf(body) == doctest::Approx(-0.5f).epsilon(0.05));
}

TEST_CASE_FIXTURE(Fixture, "RigidBody: a kinematic platform moved by code carries a box")
{
    auto& platform = scene.root()->addChild<Node3D>("Platform");
    platform.addComponent<RigidBody>().setMotion(BodyMotion::Kinematic);
    platform.addComponent<BoxCollider>().setSize({6.0f, 0.2f, 6.0f});
    const Node3D& box = crate("Box", {0.0f, 0.65f, 0.0f});
    constexpr float kSpeed = 1.0f;
    for (int frame = 0; frame < 2 * kFramesPerSecond; ++frame) {
        platform.transform().setPosition({kSpeed * kFrame * static_cast<float>(frame + 1), 0.0f, 0.0f});
        scene.advance(kFrame);
    }
    // Ящик уехал с платформой, а не остался на месте и не упал.
    CHECK(box.worldPosition().x > 1.5f);
    CHECK(heightOf(box) > 0.5f);
}

TEST_CASE_FIXTURE(ProjectFixture, "RigidBody: a matrix entry turned off lets two layers pass through each other")
{
    ground(kGroundLayer);
    auto& box = crate("Box", {0.0f, 1.0f, 0.0f});
    box.setLayer(kCrateLayer);
    project->settings().get<PhysicsSettings>().collisions().setCollides(kGroundLayer, kCrateLayer, false);
    run(2.0f);
    CHECK(heightOf(box) < -5.0f);
}

TEST_CASE_FIXTURE(Fixture, "RigidBody: a node moved by code in onFixedUpdate takes its body along in the same step")
{
    auto& box = crate("Box", {0.0f, 10.0f, 0.0f});
    box.addComponent<Teleporter>().target = {100.0f, 10.0f, 0.0f};
    scene.advance(scene.time().fixedStep());
    // Без переноса тело после шага вернуло бы узел к x = 0.
    CHECK(box.worldPosition().x == doctest::Approx(100.0f));
}

TEST_CASE_FIXTURE(Fixture, "RigidBody: an interpolated body is never pulled back by its own smoothed pose")
{
    const Node3D& smooth = crate("Smooth", {0.0f, 10.0f, 0.0f}, true);
    const Node3D& plain = crate("Plain", {5.0f, 10.0f, 0.0f}, false);
    // Кадры длиннее шага на полшага: показанное всегда между шагами.
    const float frame = 1.5f * scene.time().fixedStep();
    float previous = heightOf(smooth);
    bool falling = true;
    for (int i = 0; i < 40; ++i) {
        scene.advance(frame);
        falling = falling && heightOf(smooth) <= previous;
        previous = heightOf(smooth);
    }
    CHECK(falling);
    // Сглаженное отстаёт от последнего шага не больше чем на шаг движения.
    const float speed = kEarthGravity * 40.0f * frame;
    CHECK(heightOf(plain) <= heightOf(smooth));
    CHECK(heightOf(smooth) - heightOf(plain) <= speed * scene.time().fixedStep() + 1e-3f);
}

TEST_CASE_FIXTURE(Fixture, "RigidBody: only a running scene is stepped, the edited one keeps its nodes")
{
    Scene edited{services};
    auto& editedBox = edited.createRoot<Node3D>("Box");
    editedBox.transform().setPosition({0.0f, 10.0f, 0.0f});
    editedBox.addComponent<RigidBody>().setInterpolation(false);
    editedBox.addComponent<BoxCollider>();

    // Копия для Play - так её делает редактор.
    const SceneSerializer serializer(registry);
    Scene play{services};
    REQUIRE(serializer.fromJson(play, serializer.toJson(edited)));
    play.setPlaying(true);
    for (int i = 0; i < kFramesPerSecond; ++i) {
        play.advance(kFrame);
    }
    CHECK(heightOf(static_cast<const Node3D&>(*play.root())) < 6.0f);
    CHECK(heightOf(editedBox) == doctest::Approx(10.0f));
    CHECK(edited.system<PhysicsWorld>()->stepCount() == 0);
}

TEST_CASE_FIXTURE(ProjectFixture, "PhysicsWorld: the fixed step of the settings sets the steps per second")
{
    crate("Box", {0.0f, 10.0f, 0.0f});
    auto& time = project->settings().get<TimeSettings>();
    time.setFixedStep(1.0f / 50.0f);
    run(1.0f);
    const auto atFifty = world().stepCount();
    time.setFixedStep(1.0f / 100.0f);
    run(1.0f);
    CHECK(atFifty == doctest::Approx(50.0).epsilon(0.05));
    CHECK(world().stepCount() - atFifty == doctest::Approx(100.0).epsilon(0.05));
}
