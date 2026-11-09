#include "yqlora/scene/Timer.hpp"

#include "yqlora/scene/Node.hpp"
#include "yqlora/scene/Scene.hpp"

namespace yqlora {

TimerSystem* Timer::system() const
{
    Scene* scene = owner() ? owner()->scene() : nullptr;

    return scene ? scene->system<TimerSystem>() : nullptr;
}

void Timer::start()
{
    TimerSystem* timers = system();
    if (!timers) {
        return;
    }
    timers->cancel(m_handle);
    const auto fire = [this] {
        onTimeout.emit();
    };
    m_handle = timers->start(*this, duration, fire, {.interval = repeat ? duration : 0.0f, .unscaled = unscaled});
}

void Timer::stop()
{
    if (TimerSystem* timers = system()) {
        timers->cancel(m_handle);
    }
    m_handle = {};
}

bool Timer::running() const
{
    const TimerSystem* timers = system();

    return timers && timers->active(m_handle);
}

float Timer::remaining() const
{
    const TimerSystem* timers = system();

    return timers ? timers->remaining(m_handle).value_or(0.0f) : 0.0f;
}

void Timer::onStart()
{
    if (autostart) {
        start();
    }
}

void Timer::onStop()
{
    stop();
}

} // namespace yqlora
