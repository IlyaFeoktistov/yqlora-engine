#include "EngineRegistry.hpp"

#include <doctest/doctest.h>

#include <yqlora/core/EngineServices.hpp>
#include <yqlora/editor/BodyChainBuilder.hpp>
#include <yqlora/editor/ChainBuilder.hpp>
#include <yqlora/editor/EditorSession.hpp>
#include <yqlora/editor/SceneEditing.hpp>
#include <yqlora/input/Input.hpp>
#include <yqlora/physics/BoxCollider.hpp>
#include <yqlora/physics/CapsuleCollider.hpp>
#include <yqlora/physics/Joint.hpp>
#include <yqlora/physics/PhysicsChain.hpp>
#include <yqlora/physics/RigidBody.hpp>
#include <yqlora/render/MeshRenderer.hpp>
#include <yqlora/scene/Node3D.hpp>
#include <yqlora/scene/Scene.hpp>
#include <yqlora/scene/SceneSerializer.hpp>
#include <yqlora/scene/Transform3D.hpp>
#include <yqlora/scripting/GameModule.hpp>

#include <vector>

using namespace yqlora;
using namespace yqlora::editor;

namespace {

struct Fixture {
    Fixture()
    {
        root = static_cast<Node3D*>(session.editing().createNode(*registry.find("Node3D"), nullptr));
    }

    std::vector<Joint*> jointsOf(const Node& node)
    {
        std::vector<Joint*> joints;
        for (const auto& component : node.components()) {
            if (auto* joint = dynamic_cast<Joint*>(component.get())) {
                joints.push_back(joint);
            }
        }

        return joints;
    }

    test::EngineRegistry registry;
    SceneSerializer serializer{registry};
    Input input;
    EngineServices services{input, registry};
    GameModule gameModule{registry};
    EditorSession session{services, serializer, gameModule};
    Node3D* root = nullptr;
};

} // namespace

TEST_CASE_FIXTURE(Fixture, "Create chain: links fill the path, the ends hold on to the body under them or the world")
{
    auto& beam = root->addChild<Node3D>("Beam");
    beam.transform().setPosition({0.0f, 3.4f, 0.0f});
    beam.addComponent<BoxCollider>();

    const ChainSettings settings{
        .start = {0.0f, 3.0f, 0.0f},
        .end = {0.0f, 1.0f, 0.0f},
        .spacing = 0.25f,
        .attachStart = true,
        .attachEnd = true,
    };
    Node* chain = createChain(session, settings);
    REQUIRE(chain);
    const auto& links = chain->children();
    REQUIRE(links.size() == 8);

    const Node& first = *links.front();
    CHECK(first.getComponent<RigidBody>());
    CHECK(first.getComponent<CapsuleCollider>()->height() == doctest::Approx(0.25f));
    REQUIRE(jointsOf(first).size() == 1);
    CHECK(jointsOf(first).front()->connectedBody().id == beam.id());
    CHECK(jointsOf(*links[1]).front()->connectedBody().id == first.id());
    // Нижний конец ни на чём не лежит: держится за мир.
    const std::vector<Joint*> lastJoints = jointsOf(*links.back());
    REQUIRE(lastJoints.size() == 2);
    CHECK(lastJoints.back()->connectedBody().id == 0);

    // Звенья - по пути от начала к концу.
    session.scene().updateTransforms();
    const auto& firstLink = static_cast<const Node3D&>(first);
    const auto& lastLink = static_cast<const Node3D&>(*links.back());
    CHECK(firstLink.worldPosition().y == doctest::Approx(2.875f));
    CHECK(lastLink.worldPosition().y == doctest::Approx(1.125f));

    // Один шаг отмены убирает всю цепь, а узел до неё остаётся.
    const NodeId chainId = chain->id();
    REQUIRE(session.undo());
    CHECK(session.scene().find(chainId) == nullptr);
    CHECK(session.scene().find(root->id()) != nullptr);
}

TEST_CASE("Create chain: a free end has no joint, and a path of zero length builds nothing")
{
    const ChainSettings settings{.start = {0.0f, 2.0f, 0.0f}, .end = {1.0f, 2.0f, 0.0f}, .spacing = 0.3f};
    const std::unique_ptr<Node> chain = buildChain(settings, {});
    REQUIRE(chain);
    // Шаг уложен ровно: 1 м на 3 звена.
    REQUIRE(chain->children().size() == 3);
    std::size_t joints = 0;
    for (const auto& component : chain->children().back()->components()) {
        joints += dynamic_cast<const Joint*>(component.get()) ? 1 : 0;
    }
    CHECK(joints == 1);
    CHECK_FALSE(buildChain({.start = Vec3(1.0f), .end = Vec3(1.0f)}, {}));
}

TEST_CASE_FIXTURE(Fixture, "Create body chain: a PhysicsChain with a capsule mesh along each link, one undo step")
{
    const BodyChainSettings settings{
        .name = "Snake",
        .position = {1.0f, 0.5f, 0.0f},
        .segments = 4,
        .length = 2.0f,
        .radius = 0.1f,
        .mass = 3.0f,
    };
    Node* snake = createBodyChain(session, settings);
    REQUIRE(snake);
    CHECK(snake->name() == "Snake");
    const auto* chain = snake->getComponent<PhysicsChain>();
    REQUIRE(chain);
    CHECK(chain->segments() == 4);
    CHECK(chain->length() == doctest::Approx(2.0f));
    CHECK(chain->mass() == doctest::Approx(3.0f));
    REQUIRE(snake->children().size() == 4);

    // Меш звена лежит вдоль +Z от узла: ось капсулы (Y меша) смотрит по звену.
    session.scene().updateTransforms();
    const auto& last = static_cast<const Node3D&>(*snake->children().back());
    CHECK(last.getComponent<MeshRenderer>());
    CHECK(glm::length(last.worldPosition() - Vec3(1.0f, 0.5f, 1.75f)) < 1e-4f);
    const Vec3 meshAxis = glm::normalize(Vec3(last.worldMatrix()[1]));
    CHECK(glm::length(meshAxis - Vec3(0.0f, 0.0f, 1.0f)) < 1e-4f);

    const NodeId snakeId = snake->id();
    REQUIRE(session.undo());
    CHECK(session.scene().find(snakeId) == nullptr);
}
