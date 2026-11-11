#include "EngineRegistry.hpp"

#include <doctest/doctest.h>

#include <yqlora/core/EngineServices.hpp>
#include <yqlora/core/EngineTypes.hpp>
#include <yqlora/input/Input.hpp>
#include <yqlora/reflection/TypeRegistry.hpp>
#include <yqlora/render/Camera3D.hpp>
#include <yqlora/render/RenderSystem.hpp>
#include <yqlora/scene/Node3D.hpp>
#include <yqlora/scene/Scene.hpp>

#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

using namespace yqlora;

namespace {

using Events = std::vector<std::string>;

// Записывает, какие события жизненного цикла и в каком порядке пришли.
struct Recorder : Component {
    Recorder(Events& log, std::string name = "")
        : events(log)
        , prefix(std::move(name))
    {
    }

    YQLORA_TYPE(Recorder, Component)

    void onEnable() override
    {
        events.push_back(prefix + "enable");
    }

    void onStart() override
    {
        events.push_back(prefix + "start");
    }

    void onFixedUpdate(float /*stepSeconds*/) override
    {
        events.push_back(prefix + "fixed");
    }

    void onUpdate(float /*deltaSeconds*/) override
    {
        events.push_back(prefix + "update");
    }

    void onLateUpdate(float /*deltaSeconds*/) override
    {
        events.push_back(prefix + "late");
    }

    void onDisable() override
    {
        events.push_back(prefix + "disable");
    }

    void onStop() override
    {
        events.push_back(prefix + "stop");
    }

    Events& events;
    std::string prefix;
};

// Только onUpdate: в списках других фаз его быть не должно.
struct UpdateOnly final : Component {
    explicit UpdateOnly(Events& log)
        : events(log)
    {
    }

    YQLORA_TYPE(UpdateOnly, Component)

    void onUpdate(float /*deltaSeconds*/) override
    {
        events.push_back("update");
    }

    Events& events;
};

// Выполняет заданное действие в onUpdate: так тесты меняют сцену посреди кадра.
struct Action final : Component {
    YQLORA_TYPE(Action, Component)

    void onUpdate(float /*deltaSeconds*/) override
    {
        if (call) {
            call();
        }
    }

    std::function<void()> call;
};

// Бросает исключение из onUpdate.
struct Thrower final : Component {
    YQLORA_TYPE(Thrower, Component)

    void onUpdate(float /*deltaSeconds*/) override
    {
        throw std::runtime_error("broken script");
    }
};

// Порядок выполнения раньше остальных.
struct Early final : Recorder {
    using Recorder::Recorder;
    YQLORA_TYPE(Early, Recorder)
};

// Шаг кадра больше фиксированного шага ровно вдвое: два фиксированных шага.
constexpr float kTwoFixedSteps = 2.0f * kDefaultFixedStep;
// Десятая доля фиксированного шага: тест делает меньше десяти таких кадров
// подряд, и целый шаг в накопителе не набирается.
constexpr float kNoFixedStep = 0.1f * kDefaultFixedStep;

struct Fixture {
    Fixture()
    {
        registry.registerComponent<Recorder>().allowMultiple();
        registry.registerComponent<UpdateOnly>();
        registry.registerComponent<Early>().executionOrder(-1);
        registry.registerComponent<Thrower>();
        // Раньше остальных: успевает поменять сцену до их update.
        registry.registerComponent<Action>().executionOrder(-1);
    }

    // Журнал раньше сцены: сцена при уничтожении останавливает дерево и пишет в него.
    Events events;
    test::EngineRegistry registry;
    Input input;
    EngineServices services{input, registry};
    Scene scene{services};
};

} // namespace

TEST_CASE_FIXTURE(Fixture, "Lifecycle: start once, then fixed steps, update and late update")
{
    scene.createRoot<Node3D>().addComponent<Recorder>(events);

    scene.advance(kTwoFixedSteps);
    CHECK(events == Events{"enable", "start", "fixed", "fixed", "update", "late"});

    events.clear();
    scene.advance(kNoFixedStep);
    CHECK(events == Events{"update", "late"});

    events.clear();
    scene.stop();
    CHECK(events == Events{"disable", "stop"});

    // После остановки следующий запуск снова начинается с onEnable и onStart.
    events.clear();
    scene.advance(kNoFixedStep);
    CHECK(events == Events{"enable", "start", "update", "late"});
}

TEST_CASE_FIXTURE(Fixture, "Lifecycle: only overridden phases are called")
{
    scene.createRoot<Node3D>().addComponent<UpdateOnly>(events);
    CHECK(registry.find<UpdateOnly>()->phases() == phaseBit(LifecyclePhase::Update));

    scene.advance(kTwoFixedSteps);
    CHECK(events == Events{"update"});
}

TEST_CASE_FIXTURE(Fixture, "Lifecycle: execution order, then the order of entering")
{
    auto& root = scene.createRoot<Node3D>();
    root.addComponent<Recorder>(events, "a:");
    root.addComponent<Early>(events, "early:");
    root.addComponent<Recorder>(events, "b:");
    scene.advance(kNoFixedStep);

    // onStart - в порядке входа, фазы - по порядку выполнения типа.
    CHECK(
        events
        == Events{
            "a:enable",
            "a:start",
            "early:enable",
            "early:start",
            "b:enable",
            "b:start",
            "early:update",
            "a:update",
            "b:update",
            "early:late",
            "a:late",
            "b:late"
        }
    );
}

TEST_CASE_FIXTURE(Fixture, "Lifecycle: a component added during the game starts on its first call")
{
    auto& root = scene.createRoot<Node3D>();
    scene.advance(kNoFixedStep);

    root.addComponent<Recorder>(events);
    scene.advance(kNoFixedStep);
    CHECK(events == Events{"enable", "start", "update", "late"});
}

TEST_CASE_FIXTURE(Fixture, "Lifecycle: removing a started component or node stops it")
{
    auto& root = scene.createRoot<Node3D>();
    auto& child = root.addChild<Node3D>();
    auto& recorder = child.addComponent<Recorder>(events);
    scene.advance(kNoFixedStep);

    events.clear();
    auto owned = child.removeComponent(recorder);
    CHECK(events == Events{"disable", "stop"});
    scene.advance(kNoFixedStep);
    CHECK(events == Events{"disable", "stop"});

    child.addComponent(std::move(owned));
    scene.advance(kNoFixedStep);
    events.clear();
    auto removedNode = root.removeChild(child);
    CHECK(events == Events{"disable", "stop"});
}

TEST_CASE_FIXTURE(Fixture, "Lifecycle: taking the tree out of the scene stops it")
{
    scene.createRoot<Node3D>().addComponent<Recorder>(events);
    scene.advance(kNoFixedStep);

    events.clear();
    auto root = scene.takeRoot();
    CHECK(events == Events{"disable", "stop"});
}

TEST_CASE_FIXTURE(Fixture, "Lifecycle: nothing happens without advance")
{
    scene.createRoot<Node3D>().addComponent<Recorder>(events);
    scene.stop();
    CHECK(events.empty());
}

TEST_CASE_FIXTURE(Fixture, "Lifecycle: a disabled component takes no part until enabled")
{
    auto& recorder = scene.createRoot<Node3D>().addComponent<Recorder>(events);
    recorder.setEnabled(false);
    scene.advance(kNoFixedStep);
    CHECK(events.empty());

    recorder.setEnabled(true);
    scene.advance(kNoFixedStep);
    CHECK(events == Events{"enable", "start", "update", "late"});

    // Выключение сразу даёт onDisable; повторное включение - onEnable, но не второй onStart.
    events.clear();
    recorder.setEnabled(false);
    CHECK(events == Events{"disable"});
    scene.advance(kNoFixedStep);
    recorder.setEnabled(true);
    scene.advance(kNoFixedStep);
    CHECK(events == Events{"disable", "enable", "update", "late"});
}

TEST_CASE_FIXTURE(Fixture, "Lifecycle: an inactive node switches off its whole branch")
{
    auto& root = scene.createRoot<Node3D>();
    auto& parent = root.addChild<Node3D>();
    auto& child = parent.addChild<Node3D>();
    child.addComponent<Recorder>(events, "child:");
    scene.advance(kNoFixedStep);

    events.clear();
    parent.setActive(false);
    CHECK(events == Events{"child:disable"});
    CHECK(child.active());
    CHECK_FALSE(child.activeInHierarchy());
    scene.advance(kNoFixedStep);
    CHECK(events == Events{"child:disable"});

    // Свой флаг ребёнка выключен: активация родителя его не включает.
    child.setActive(false);
    parent.setActive(true);
    CHECK_FALSE(child.activeInHierarchy());
    scene.advance(kNoFixedStep);
    CHECK(events == Events{"child:disable"});

    events.clear();
    child.setActive(true);
    scene.advance(kNoFixedStep);
    CHECK(events == Events{"child:enable", "child:update", "child:late"});
}

TEST_CASE_FIXTURE(Fixture, "Lifecycle: a node moved under an inactive parent is switched off")
{
    auto& root = scene.createRoot<Node3D>();
    auto& inactive = root.addChild<Node3D>();
    inactive.setActive(false);
    auto& node = root.addChild<Node3D>();
    node.addComponent<Recorder>(events);
    scene.advance(kNoFixedStep);

    events.clear();
    inactive.addChild(root.removeChild(node));
    scene.advance(kNoFixedStep);
    CHECK(events == Events{"disable", "stop"});
    CHECK_FALSE(node.activeInHierarchy());
}

TEST_CASE_FIXTURE(Fixture, "Lifecycle: an exception disables the component, the game goes on")
{
    auto& root = scene.createRoot<Node3D>();
    auto& thrower = root.addComponent<Thrower>();
    root.addComponent<Recorder>(events);

    scene.advance(kNoFixedStep);
    CHECK_FALSE(thrower.enabled());
    CHECK(events == Events{"enable", "start", "update", "late"});

    events.clear();
    scene.advance(kNoFixedStep);
    CHECK(events == Events{"update", "late"});
}

TEST_CASE_FIXTURE(Fixture, "Tree changes: destroying during a frame happens at its end")
{
    auto& root = scene.createRoot<Node3D>();
    auto& doomed = root.addChild<Node3D>();
    doomed.addComponent<Recorder>(events);
    auto& action = root.addComponent<Action>();
    scene.advance(kNoFixedStep);

    events.clear();
    action.call = [&] {
        scene.destroy(doomed);
        CHECK(root.children().size() == 1);
        action.call = nullptr;
    };
    scene.advance(kNoFixedStep);
    CHECK(events == Events{"update", "late", "disable", "stop"});
    CHECK(root.children().empty());
}

TEST_CASE_FIXTURE(Fixture, "Tree changes: moving keeps components running")
{
    auto& root = scene.createRoot<Node3D>();
    auto& from = root.addChild<Node3D>();
    auto& to = root.addChild<Node3D>();
    auto& node = from.addChild<Node3D>();
    node.addComponent<Recorder>(events);
    auto& action = root.addComponent<Action>();
    scene.advance(kNoFixedStep);

    events.clear();
    action.call = [&] {
        scene.setParent(node, to, KeepWorld::No);
        CHECK(node.parent() == &from);
        action.call = nullptr;
    };
    scene.advance(kNoFixedStep);
    CHECK(node.parent() == &to);
    CHECK(from.children().empty());
    CHECK(events == Events{"update", "late"});

    // Вне кадра - сразу; под неактивным родителем компонент выключается.
    events.clear();
    from.setActive(false);
    scene.setParent(node, from, KeepWorld::No);
    CHECK(node.parent() == &from);
    CHECK(events == Events{"disable"});
}

TEST_CASE_FIXTURE(Fixture, "Tree changes: a node created during a frame takes part from the next one")
{
    auto& root = scene.createRoot<Node3D>();
    auto& action = root.addComponent<Action>();
    scene.advance(kNoFixedStep);

    action.call = [&] {
        root.addChild<Node3D>().addComponent<Recorder>(events);
        action.call = nullptr;
    };
    scene.advance(kNoFixedStep);
    CHECK(events.empty());
    scene.advance(kNoFixedStep);
    CHECK(events == Events{"enable", "start", "update", "late"});
}

TEST_CASE_FIXTURE(Fixture, "Tree changes: destroyed twice, or with the branch, is applied once")
{
    auto& root = scene.createRoot<Node3D>();
    auto& parent = root.addChild<Node3D>();
    auto& child = parent.addChild<Camera3D>();
    auto& action = root.addComponent<Action>();
    action.call = [&] {
        scene.destroy(child);
        scene.destroy(parent);
        scene.destroy(child);
        action.call = nullptr;
    };
    scene.advance(kNoFixedStep);
    CHECK(root.children().empty());
    // Камера ушла вместе с веткой и выпала из списка камер.
    CHECK(scene.system<RenderSystem>()->mainCamera() == nullptr);
}

TEST_CASE_FIXTURE(Fixture, "Tree changes: outside a frame the root is destroyed at once")
{
    scene.createRoot<Node3D>().addComponent<Recorder>(events);
    scene.advance(kNoFixedStep);

    events.clear();
    scene.destroy(*scene.root());
    CHECK(scene.empty());
    CHECK(events == Events{"disable", "stop"});
}

TEST_CASE("Lifecycle: a scene without a registry calls nobody")
{
    Events events;
    Scene scene{EngineServices::none()};
    scene.createRoot<Node3D>().addComponent<UpdateOnly>(events);
    scene.advance(kNoFixedStep);
    CHECK(events.empty());
}

TEST_CASE("GameTime: scale, fixed steps and their limit")
{
    GameTime time;
    time.beginFrame(0.1f);
    CHECK(time.delta() == doctest::Approx(0.1f));
    CHECK(time.frame() == 1);

    time.setScale(0.5f);
    time.beginFrame(0.1f);
    CHECK(time.delta() == doctest::Approx(0.05f));
    CHECK(time.unscaledDelta() == doctest::Approx(0.1f));
    CHECK(time.elapsed() == doctest::Approx(0.15));

    // Стоп: время стоит, шагов нет.
    GameTime stopped;
    stopped.setScale(0.0f);
    stopped.beginFrame(1.0f);
    CHECK(stopped.delta() == 0.0f);
    CHECK_FALSE(stopped.takeFixedStep());

    // После очень длинного кадра шагов не больше предела.
    GameTime slow;
    slow.beginFrame(10.0f);
    int steps = 0;
    while (slow.takeFixedStep()) {
        ++steps;
    }
    CHECK(steps == kMaxFixedStepsPerFrame);
}

namespace {

// Записывает шаги кадра системы рядом с событиями компонентов.
struct RecordingSystem final : SceneSystem {
    YQLORA_TYPE(RecordingSystem, SceneSystem)

    using SceneSystem::SceneSystem;

    void fixedUpdate(float /*stepSeconds*/) override
    {
        log->push_back("system:fixed");
    }

    void afterFixedUpdate(float /*stepSeconds*/) override
    {
        log->push_back("system:afterFixed");
    }

    void update(float /*deltaSeconds*/) override
    {
        log->push_back("system:update");
    }

    void lateUpdate(float /*deltaSeconds*/) override
    {
        log->push_back("system:late");
    }

    Events* log = nullptr;
};

} // namespace

TEST_CASE("SceneSystem: every scene gets its own, steps run next to the component phases")
{
    Events events;
    TypeRegistry registry;
    registerEngineTypes(registry);
    registry.registerComponent<Recorder>();
    registry.registerSystem<RecordingSystem>();
    Input input;
    EngineServices services{input, registry};
    Scene scene{services};
    Scene other{services};

    auto* system = scene.system<RecordingSystem>();
    REQUIRE(system);
    CHECK(system != other.system<RecordingSystem>());
    system->log = &events;

    scene.createRoot<Node3D>().addComponent<Recorder>(events);
    scene.advance(kTwoFixedSteps);
    CHECK(
        events
        == Events{
            "enable",
            "start",
            "system:fixed",
            "fixed",
            "system:afterFixed",
            "system:fixed",
            "fixed",
            "system:afterFixed",
            "update",
            "system:update",
            "late",
            "system:late"
        }
    );
}
