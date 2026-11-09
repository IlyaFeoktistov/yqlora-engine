#include "EngineRegistry.hpp"

#include <doctest/doctest.h>

#include <yqlora/core/EngineServices.hpp>
#include <yqlora/input/Input.hpp>
#include <yqlora/scene/Node3D.hpp>
#include <yqlora/scene/Scene.hpp>
#include <yqlora/scene/TimerSystem.hpp>
#include <yqlora/scripting/Script.hpp>

#include <stdexcept>
#include <string>
#include <vector>

using namespace yqlora;

namespace {

// Кадр короче предела GameTime (kDefaultMaxFrame), чтобы время не обрезалось.
constexpr float kFrame = 0.05f;

// Скрипт, который заводит таймеры: владелец для системы и доступ через timers().
struct Clock final : Script {
    YQLORA_TYPE(Clock, Script)

    // Скрипт зовёт timers() сам; тест - снаружи, от его имени.
    using Script::timers;
};

struct Fixture {
    Fixture()
    {
        registry.registerComponent<Clock>().allowMultiple();
        clock = &scene.createRoot<Node3D>("Root").addComponent<Clock>();
        timers = scene.system<TimerSystem>();
        REQUIRE(timers);
    }

    // Кадры по kFrame, пока не пройдёт seconds настоящего времени.
    void run(float seconds)
    {
        const int frames = static_cast<int>(seconds / kFrame + 0.5f);
        for (int i = 0; i < frames; ++i) {
            scene.advance(kFrame);
        }
    }

    std::vector<std::string> calls;
    test::EngineRegistry registry;
    Input input;
    EngineServices services{input, registry};
    Scene scene{services};
    Clock* clock = nullptr;
    TimerSystem* timers = nullptr;
};

} // namespace

TEST_CASE_FIXTURE(Fixture, "Timers: a one-shot fires once at its time, on game time with its scale")
{
    scene.time().setScale(0.5f);
    clock->timers().after(1.0f, [&] {
        calls.push_back("once");
    });
    // Игровое время идёт вдвое медленнее: секунда настоящего - полсекунды игры.
    run(1.0f);
    CHECK(calls.empty());
    // Кадр сверх ровной секунды: сумма шагов float у самой границы может недобрать.
    run(1.0f + kFrame);
    CHECK(calls == std::vector<std::string>{"once"});
    run(2.0f);
    CHECK(calls.size() == 1);
}

TEST_CASE_FIXTURE(Fixture, "Timers: a repeated one catches up when the frame is longer than its interval")
{
    int fired = 0;
    clock->timers().every(0.01f, [&] {
        ++fired;
    });
    // Пять интервалов в одном кадре - пять срабатываний.
    scene.advance(kFrame);
    CHECK(fired == 5);
    scene.advance(kFrame);
    CHECK(fired == 10);
}

TEST_CASE_FIXTURE(Fixture, "Timers: pause, game time scale 0 and real-time timers")
{
    int scaled = 0;
    int unscaled = 0;
    const TimerHandle handle = clock->timers().every(0.1f, [&] {
        ++scaled;
    });
    clock->timers().every(
        0.1f,
        [&] {
            ++unscaled;
        },
        true
    );
    clock->timers().pause(handle);
    run(0.5f);
    CHECK(scaled == 0);
    CHECK(unscaled == 5);
    clock->timers().resume(handle);
    scene.time().setScale(0.0f);
    run(0.5f);
    // Игра стоит: идут только таймеры настоящего времени (меню паузы).
    CHECK(scaled == 0);
    CHECK(unscaled == 10);
    scene.time().setScale(1.0f);
    run(0.5f);
    CHECK(scaled == 5);
}

TEST_CASE_FIXTURE(Fixture, "Timers: a destroyed owner is never called, a disabled one waits")
{
    auto& other = scene.root()->addChild<Node3D>("Other").addComponent<Clock>();
    other.timers().after(0.2f, [&] {
        calls.push_back("destroyed");
    });
    scene.destroy(*other.owner());
    run(0.5f);
    CHECK(calls.empty());

    const TimerHandle handle = clock->timers().after(0.3f, [&] {
        calls.push_back("disabled");
    });
    run(0.1f);
    clock->setEnabled(false);
    run(1.0f);
    CHECK(calls.empty());
    // Ждал выключенным: осталось столько же, сколько было.
    CHECK(*timers->remaining(handle) == doctest::Approx(0.2f));
    clock->setEnabled(true);
    run(0.2f + kFrame);
    CHECK(calls == std::vector<std::string>{"disabled"});
    CHECK_FALSE(timers->active(handle));
}

TEST_CASE_FIXTURE(Fixture, "Timers: order by due time, new ones wait for the next frame, a throw is logged")
{
    clock->timers().after(0.03f, [&] {
        calls.push_back("later");
    });
    clock->timers().after(0.01f, [&] {
        calls.push_back("earlier");
        // Заведённый в обработчике сработает не раньше следующего кадра.
        clock->timers().after(0.0f, [&] {
            calls.push_back("next frame");
        });
        throw std::runtime_error("broken timer");
    });
    const TimerHandle cancelled = clock->timers().after(0.04f, [&] {
        calls.push_back("cancelled");
    });
    clock->timers().after(0.02f, [&] {
        calls.push_back("middle");
        clock->timers().cancel(cancelled);
    });
    scene.advance(kFrame);
    CHECK(calls == std::vector<std::string>{"earlier", "middle", "later"});
    scene.advance(kFrame);
    CHECK(calls.back() == "next frame");
}
