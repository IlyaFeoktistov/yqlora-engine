#include "EngineRegistry.hpp"

#include <doctest/doctest.h>

#include <yqlora/core/EngineServices.hpp>
#include <yqlora/input/Input.hpp>
#include <yqlora/reflection/TypeRegistry.hpp>
#include <yqlora/scene/Event.hpp>
#include <yqlora/scene/Node3D.hpp>
#include <yqlora/scene/Scene.hpp>

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

using namespace yqlora;

namespace {

// Сообщает о набранных очках.
struct Score final : Component {
    YQLORA_TYPE(Score, Component)

    Event<int, std::string> onScored;
};

// Подписчик: сам по себе ничего не делает, нужен как владелец подписок.
struct Listener final : Component {
    YQLORA_TYPE(Listener, Component)
};

struct Fixture {
    Fixture()
    {
        registry.registerComponent<Score>().event("onScored", &Score::onScored, {"points", "reason"});
        registry.registerComponent<Listener>().allowMultiple();
        auto& root = scene.createRoot<Node3D>("Root");
        score = &root.addChild<Node3D>("Scorer").addComponent<Score>();
        listenerNode = &root.addChild<Node3D>("Listener");
        listener = &listenerNode->addComponent<Listener>();
    }

    test::EngineRegistry registry;
    Input input;
    EngineServices services{input, registry};
    Scene scene{services};
    Score* score = nullptr;
    Node3D* listenerNode = nullptr;
    Listener* listener = nullptr;
    std::vector<std::string> calls;
};

} // namespace

TEST_CASE_FIXTURE(Fixture, "Event: listeners get the arguments in the order they connected")
{
    Connection first = score->onScored.connect(*listener, [&](int points, const std::string& reason) {
        calls.push_back("first " + std::to_string(points) + " " + reason);
    });
    Connection second = score->onScored.connect(*listener, [&](int points, const std::string&) {
        calls.push_back("second " + std::to_string(points));
    });
    score->onScored.emit(10, "coin");
    CHECK(calls == std::vector<std::string>{"first 10 coin", "second 10"});
    CHECK(score->onScored.listenerCount() == 2);
}

TEST_CASE_FIXTURE(Fixture, "Event: a callback of an owner that left the scene is never called")
{
    Connection kept = score->onScored.connect(*listener, [&](int, const std::string&) {
        calls.push_back("listener");
    });
    kept.release();
    CHECK(score->onScored.listenerCount() == 1);

    // Узел владельца ушёл из сцены и вернулся: подписка не оживает.
    auto taken = listenerNode->parent()->removeChild(*listenerNode);
    score->onScored.emit(1, "");
    CHECK(calls.empty());
    CHECK(score->onScored.listenerCount() == 0);
    scene.root()->addChild(std::move(taken));
    score->onScored.emit(1, "");
    CHECK(calls.empty());

    // Снятый компонент и уничтоженный узел - то же самое.
    auto& other = listenerNode->addComponent<Listener>();
    score->onScored
        .connect(
            other,
            [&](int, const std::string&) {
                calls.push_back("other");
            }
        )
        .release();
    listenerNode->removeComponent(other);
    score->onScored.emit(1, "");
    CHECK(calls.empty());

    auto& third = listenerNode->addComponent<Listener>();
    score->onScored
        .connect(
            third,
            [&](int, const std::string&) {
                calls.push_back("third");
            }
        )
        .release();
    scene.destroy(*listenerNode);
    score->onScored.emit(1, "");
    CHECK(calls.empty());
}

TEST_CASE_FIXTURE(Fixture, "Event: connecting and disconnecting during an emit")
{
    std::optional<Connection> late;
    Connection second;
    Connection first = score->onScored.connect(*listener, [&](int, const std::string&) {
        calls.push_back("first");
        // Новая подписка ждёт следующего срабатывания, снятая не вызывается.
        if (!late) {
            late = score->onScored.connect(*listener, [&](int, const std::string&) {
                calls.push_back("late");
            });
        }
        second.disconnect();
    });
    second = score->onScored.connect(*listener, [&](int, const std::string&) {
        calls.push_back("second");
    });
    score->onScored.emit(1, "");
    CHECK(calls == std::vector<std::string>{"first"});
    calls.clear();
    score->onScored.emit(1, "");
    CHECK(calls == std::vector<std::string>{"first", "late"});
}

TEST_CASE_FIXTURE(Fixture, "Event: an exception in one listener does not stop the next")
{
    Connection thrower = score->onScored.connect(*listener, [](int, const std::string&) {
        throw std::runtime_error("broken listener");
    });
    Connection next = score->onScored.connect(*listener, [&](int, const std::string&) {
        calls.push_back("next");
    });
    score->onScored.emit(1, "");
    CHECK(calls == std::vector<std::string>{"next"});
}

TEST_CASE_FIXTURE(Fixture, "Connection: destroyed it disconnects, released it lives with the owner")
{
    {
        Connection scoped = score->onScored.connect(*listener, [&](int, const std::string&) {
            calls.push_back("scoped");
        });
        CHECK(scoped.connected());
    }
    score->onScored.emit(1, "");
    CHECK(calls.empty());

    Connection moved = score->onScored.connect(*listener, [&](int, const std::string&) {
        calls.push_back("moved");
    });
    Connection target = std::move(moved);
    CHECK_FALSE(moved.connected());
    CHECK(target.connected());
    score->onScored.emit(1, "");
    CHECK(calls == std::vector<std::string>{"moved"});
    target.disconnect();
    CHECK_FALSE(target.connected());

    // Событие ушло раньше подписки: отписка ничего не ломает.
    Connection orphan;
    {
        Event<> local;
        orphan = local.connect(*listener, [] {
        });
        CHECK(orphan.connected());
    }
    CHECK_FALSE(orphan.connected());
    orphan.disconnect();
}

TEST_CASE_FIXTURE(Fixture, "Event: by registry name, with arguments as property values")
{
    const TypeInfo* type = registry.find("Score");
    REQUIRE(type);
    const auto events = registry.events(*type);
    REQUIRE(events.size() == 1);
    const EventInfo& info = *events.front();
    CHECK(info.name == "onScored");
    REQUIRE(info.parameters.size() == 2);
    CHECK(info.parameters[0].name == "points");
    CHECK(info.parameters[0].type == PropertyType::Int);
    CHECK(info.parameters[1].type == PropertyType::String);

    std::vector<PropertyValue> received;
    Connection connection = info.of(*score).connectValues(*listener, [&](std::span<const PropertyValue> values) {
        received.assign(values.begin(), values.end());
    });
    score->onScored.emit(7, "chest");
    CHECK(received == std::vector<PropertyValue>{7, std::string("chest")});
}
