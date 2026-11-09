#include "yqlora/scene/TimerSystem.hpp"

#include "yqlora/core/Log.hpp"
#include "yqlora/scene/Component.hpp"
#include "yqlora/scene/Scene.hpp"

#include <algorithm>
#include <exception>
#include <string_view>

namespace yqlora {

struct TimerSystem::Timer {
    std::uint64_t id = 0;
    // Секунд до срабатывания; после сдвига кадра может уйти в минус.
    float remaining = 0.0f;
    float interval = 0.0f;
    bool unscaled = false;
    bool paused = false;
    bool finished = false;
    std::weak_ptr<const void> lifetime;
    Component* owner = nullptr;
    Callback callback;
};

TimerSystem::TimerSystem(Scene& scene)
    : SceneSystem(scene)
{
}

TimerSystem::~TimerSystem() = default;

TimerHandle TimerSystem::start(Component& owner, float seconds, Callback callback, Options options)
{
    auto timer = std::make_unique<Timer>();
    timer->id = m_nextId++;
    timer->remaining = std::max(seconds, 0.0f);
    timer->interval = std::max(options.interval, 0.0f);
    timer->unscaled = options.unscaled;
    timer->lifetime = owner.lifetime();
    timer->owner = &owner;
    timer->callback = std::move(callback);
    const TimerHandle handle{timer->id};
    m_timers.push_back(std::move(timer));

    return handle;
}

TimerSystem::Timer* TimerSystem::find(TimerHandle timer) const
{
    const auto it = std::ranges::find_if(m_timers, [&](const auto& t) {
        return t->id == timer.id && !t->finished;
    });

    return it != m_timers.end() ? it->get() : nullptr;
}

void TimerSystem::cancel(TimerHandle timer)
{
    if (Timer* found = find(timer)) {
        found->finished = true;
    }
}

void TimerSystem::pause(TimerHandle timer)
{
    if (Timer* found = find(timer)) {
        found->paused = true;
    }
}

void TimerSystem::resume(TimerHandle timer)
{
    if (Timer* found = find(timer)) {
        found->paused = false;
    }
}

std::optional<float> TimerSystem::remaining(TimerHandle timer) const
{
    const Timer* found = find(timer);

    return found && !found->lifetime.expired() ? std::optional(std::max(found->remaining, 0.0f)) : std::nullopt;
}

bool TimerSystem::active(TimerHandle timer) const
{
    return remaining(timer).has_value();
}

void TimerSystem::update(float deltaSeconds)
{
    const float unscaledDelta = scene().time().unscaledDelta();
    // Одно срабатывание: таймер и насколько оно запоздало (чем больше, тем раньше наступило).
    struct Firing {
        std::uint64_t id = 0;
        float late = 0.0f;
    };
    std::vector<Firing> firings;
    // Заведённые в обработчиках ложатся в конец и в этом кадре не считаются.
    const std::size_t count = m_timers.size();
    for (std::size_t i = 0; i < count; ++i) {
        Timer& timer = *m_timers[i];
        if (timer.finished || timer.lifetime.expired()) {
            timer.finished = true;
            continue;
        }
        if (timer.paused || !timer.owner->activeAndEnabled()) {
            continue;
        }
        timer.remaining -= timer.unscaled ? unscaledDelta : deltaSeconds;
        while (timer.remaining <= 0.0f) {
            firings.push_back({timer.id, -timer.remaining});
            if (timer.interval <= 0.0f) {
                break;
            }
            timer.remaining += timer.interval;
        }
    }
    // Раньше наступившее - раньше; при равном - порядок создания (ID растут).
    std::ranges::stable_sort(firings, [](const Firing& a, const Firing& b) {
        return a.late != b.late ? a.late > b.late : a.id < b.id;
    });
    for (const Firing& firing : firings) {
        Timer* timer = find(TimerHandle{firing.id});
        // Обработчик раньше мог снять таймер или уничтожить владельца.
        if (!timer || timer->lifetime.expired()) {
            continue;
        }
        if (timer->interval <= 0.0f) {
            timer->finished = true;
        }
        const std::string_view ownerType = timer->owner->typeName();
        // Копия: обработчик может завести таймер, и список переложится.
        const Callback callback = timer->callback;
        try {
            callback();
        } catch (const std::exception& error) {
            log::error("Timer of component '{}' threw: {}", ownerType, error.what());
        } catch (...) {
            log::error("Timer of component '{}' threw an unknown exception", ownerType);
        }
    }
    std::erase_if(m_timers, [](const auto& timer) {
        return timer->finished || timer->lifetime.expired();
    });
}

TimerHandle Timers::after(float seconds, TimerSystem::Callback callback, bool unscaled)
{
    return m_system.start(m_owner, seconds, std::move(callback), {.interval = 0.0f, .unscaled = unscaled});
}

TimerHandle Timers::every(float seconds, TimerSystem::Callback callback, bool unscaled)
{
    return m_system.start(m_owner, seconds, std::move(callback), {.interval = seconds, .unscaled = unscaled});
}

} // namespace yqlora
