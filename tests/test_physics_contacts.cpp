#include "EngineRegistry.hpp"

#include <doctest/doctest.h>

#include <yqlora/core/EngineServices.hpp>
#include <yqlora/input/Input.hpp>
#include <yqlora/physics/BoxCollider.hpp>
#include <yqlora/physics/Collision.hpp>
#include <yqlora/physics/RigidBody.hpp>
#include <yqlora/physics/SphereCollider.hpp>
#include <yqlora/scene/Node3D.hpp>
#include <yqlora/scene/Scene.hpp>
#include <yqlora/scene/SceneSerializer.hpp>
#include <yqlora/scene/Transform3D.hpp>

#include <glm/geometric.hpp>

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

    // Пол с неподвижным RigidBody: он тоже получает события столкновений.
    Node3D& ground()
    {
        auto& node = scene.root()->addChild<Node3D>("Ground");
        node.transform().setPosition({0.0f, -0.5f, 0.0f});
        node.addComponent<RigidBody>().setMotion(BodyMotion::Static);
        node.addComponent<BoxCollider>().setSize({40.0f, 1.0f, 40.0f});

        return node;
    }

    Node3D& body(const std::string& name, Vec3 position)
    {
        auto& node = scene.root()->addChild<Node3D>(name);
        node.transform().setPosition(position);
        node.addComponent<RigidBody>().setInterpolation(false);

        return node;
    }

    Node3D& zone(const std::string& name, Vec3 position, Vec3 size)
    {
        auto& node = scene.root()->addChild<Node3D>(name);
        node.transform().setPosition(position);
        auto& collider = node.addComponent<BoxCollider>();
        collider.setSize(size);
        collider.setTrigger(true);

        return node;
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

// Счётчики событий столкновений одного тела.
struct CollisionLog {
    explicit CollisionLog(RigidBody& body)
    {
        body.onCollisionEnter
            .connect(
                body,
                [this](const Collision& collision) {
                    ++enters;
                    lastEnter = collision;
                }
            )
            .release();
        body.onCollisionStay
            .connect(
                body,
                [this](const Collision&) {
                    ++stays;
                }
            )
            .release();
        body.onCollisionExit
            .connect(
                body,
                [this](const Collision& collision) {
                    ++exits;
                    lastExit = collision;
                }
            )
            .release();
    }

    int enters = 0;
    int stays = 0;
    int exits = 0;
    Collision lastEnter;
    Collision lastExit;
};

struct TriggerLog {
    explicit TriggerLog(Collider& trigger)
    {
        trigger.onTriggerEnter
            .connect(
                trigger,
                [this](NodeRef other) {
                    entered.push_back(other.id);
                }
            )
            .release();
        trigger.onTriggerExit
            .connect(
                trigger,
                [this](NodeRef other) {
                    exited.push_back(other.id);
                }
            )
            .release();
    }

    std::vector<NodeId> entered;
    std::vector<NodeId> exited;
};

} // namespace

TEST_CASE_FIXTURE(Fixture, "Contacts: one landing enters once, stays while awake, sleeps without exit, exits once")
{
    Node3D& floor = ground();
    Node3D& box = body("Box", {0.0f, 2.0f, 0.0f});
    box.addComponent<BoxCollider>();
    CollisionLog log(*box.getComponent<RigidBody>());
    CollisionLog floorLog(*floor.getComponent<RigidBody>());

    // Тело ложится и засыпает: касание продолжается без новых событий.
    run(4.0f);
    CHECK(log.enters == 1);
    CHECK(log.stays > 0);
    CHECK(log.exits == 0);
    CHECK(floorLog.enters == 1);
    CHECK(log.lastEnter.other.id == floor.id());
    CHECK(log.lastEnter.collider.component == box.getComponent<BoxCollider>()->id());
    CHECK(log.lastEnter.otherCollider.component == floor.getComponent<BoxCollider>()->id());
    CHECK_FALSE(log.lastEnter.points.empty());
    CHECK(glm::dot(log.lastEnter.normal, Vec3(0.0f, 1.0f, 0.0f)) > 0.99f);
    CHECK(glm::dot(floorLog.lastEnter.normal, Vec3(0.0f, -1.0f, 0.0f)) > 0.99f);
    // Падение с 1.5 м: скорость удара около 5.4 м/с, масса 1 кг.
    CHECK(log.lastEnter.impulse > 3.0f);
    CHECK(log.lastEnter.relativeVelocity.y < -3.0f);
    const int staysAsleep = log.stays;
    run(1.0f);
    CHECK(log.stays == staysAsleep);

    box.setWorldPosition({0.0f, 10.0f, 0.0f});
    run(0.1f);
    CHECK(log.enters == 1);
    CHECK(log.exits == 1);
    CHECK(floorLog.exits == 1);
    CHECK(log.lastExit.points.empty());
}

TEST_CASE_FIXTURE(Fixture, "Contacts: a trigger reports a body passing through and does not push it")
{
    Node3D& gate = zone("Gate", {0.0f, 5.0f, 0.0f}, {4.0f, 1.0f, 4.0f});
    TriggerLog log(*gate.getComponent<BoxCollider>());
    Node3D& ball = body("Ball", {0.0f, 8.0f, 0.0f});
    ball.addComponent<SphereCollider>().setRadius(0.25f);
    Node3D& free = body("Free", {10.0f, 8.0f, 0.0f});
    free.addComponent<SphereCollider>().setRadius(0.25f);

    run(2.0f);
    REQUIRE(log.entered.size() == 1);
    CHECK(log.entered.front() == ball.id());
    REQUIRE(log.exited.size() == 1);
    CHECK(log.exited.front() == ball.id());
    CHECK(ball.worldPosition().y == doctest::Approx(free.worldPosition().y).epsilon(0.001));
}

TEST_CASE_FIXTURE(Fixture, "Contacts: a body asleep inside a trigger stays inside")
{
    ground();
    Node3D& area = zone("Area", {0.0f, 1.0f, 0.0f}, {4.0f, 2.0f, 4.0f});
    TriggerLog log(*area.getComponent<BoxCollider>());
    Node3D& box = body("Box", {0.0f, 1.5f, 0.0f});
    box.addComponent<BoxCollider>();
    run(5.0f);
    CHECK(log.entered.size() == 1);
    CHECK(log.exited.empty());
}

TEST_CASE_FIXTURE(Fixture, "Contacts: a trigger sees static bodies only when asked")
{
    Node3D& floor = ground();
    Node3D& blind = zone("Blind", {0.0f, 0.0f, 0.0f}, {2.0f, 2.0f, 2.0f});
    Node3D& seeing = zone("Seeing", {5.0f, 0.0f, 0.0f}, {2.0f, 2.0f, 2.0f});
    seeing.getComponent<BoxCollider>()->setDetectStatic(true);
    TriggerLog blindLog(*blind.getComponent<BoxCollider>());
    TriggerLog seeingLog(*seeing.getComponent<BoxCollider>());
    run(0.2f);
    CHECK(blindLog.entered.empty());
    REQUIRE(seeingLog.entered.size() == 1);
    CHECK(seeingLog.entered.front() == floor.id());
}

TEST_CASE_FIXTURE(Fixture, "Contacts: a child collider's collision reaches its body with that collider")
{
    ground();
    Node3D& robot = body("Robot", {0.0f, 3.0f, 0.0f});
    auto& foot = robot.addChild<Node3D>("Foot");
    foot.transform().setPosition({0.0f, -1.0f, 0.0f});
    const auto& footCollider = foot.addComponent<BoxCollider>();
    CollisionLog log(*robot.getComponent<RigidBody>());
    run(2.0f);
    REQUIRE(log.enters >= 1);
    CHECK(log.lastEnter.collider.node == foot.id());
    CHECK(log.lastEnter.collider.component == footCollider.id());
}

TEST_CASE_FIXTURE(Fixture, "Contacts: a body destroyed in its own handler leaves, the other side gets the exit")
{
    Node3D& floor = ground();
    Node3D& box = body("Box", {0.0f, 2.0f, 0.0f});
    box.addComponent<BoxCollider>();
    auto& rigid = *box.getComponent<RigidBody>();
    rigid.onCollisionEnter
        .connect(
            rigid,
            [&](const Collision&) {
                scene.destroy(box);
            }
        )
        .release();
    CollisionLog floorLog(*floor.getComponent<RigidBody>());
    run(3.0f);
    CHECK(floorLog.enters == 1);
    CHECK(floorLog.exits == 1);
    CHECK(floorLog.lastExit.other.id != 0);
    CHECK_FALSE(scene.find(floorLog.lastExit.other));
}

TEST_CASE("Contacts: the same events in the same order on every run of a saved scene")
{
    // ID узлов выдаются случайно, а у сохранённой сцены они постоянные:
    // порядок по ID повторяется при каждой загрузке.
    std::string saved;
    {
        Fixture fixture;
        fixture.ground();
        for (int i = 0; i < 6; ++i) {
            const Vec3 position{0.3f * static_cast<float>(i), 1.0f + 1.2f * static_cast<float>(i), 0.0f};
            fixture.body("Box" + std::to_string(i), position).addComponent<BoxCollider>();
        }
        saved = SceneSerializer(fixture.registry).toJson(fixture.scene);
    }
    std::vector<std::string> first;
    for (int run = 0; run < 10; ++run) {
        Fixture fixture;
        REQUIRE(SceneSerializer(fixture.registry).fromJson(fixture.scene, saved));
        fixture.scene.setPlaying(true);
        std::vector<std::string> order;
        for (const auto& child : fixture.scene.root()->children()) {
            auto* rigid = child->getComponent<RigidBody>();
            if (!rigid) {
                continue;
            }
            const std::string name = child->name();
            const auto otherName = [&fixture](const Collision& collision) {
                const Node* other = fixture.scene.find(collision.other);

                return other ? other->name() : std::string("?");
            };
            rigid->onCollisionEnter
                .connect(
                    *rigid,
                    [&order, name, otherName](const Collision& collision) {
                        order.push_back(name + " > " + otherName(collision));
                    }
                )
                .release();
            rigid->onCollisionExit
                .connect(
                    *rigid,
                    [&order, name, otherName](const Collision& collision) {
                        order.push_back(name + " < " + otherName(collision));
                    }
                )
                .release();
        }
        fixture.run(3.0f);
        REQUIRE_FALSE(order.empty());
        if (run == 0) {
            first = order;
        } else {
            CHECK(order == first);
        }
    }
}
