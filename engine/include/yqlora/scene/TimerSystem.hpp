#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/reflection/TypeName.hpp"
#include "yqlora/scene/SceneSystem.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace yqlora {

class Component;

// Таймер сцены; 0 - таймера нет.
struct TimerHandle {
    std::uint64_t id = 0;

    bool operator==(const TimerHandle&) const = default;
};

// Таймеры сцены: "через 3 секунды" и "каждые 0.5 секунды". Срабатывают в
// systems.update, после onUpdate компонентов, по игровому времени (пауза и
// масштаб времени действуют) или по настоящему (unscaled: интерфейс во время
// паузы).
//
// У таймера есть владелец - компонент, который его завёл: таймер снимается,
// когда владелец уходит из сцены, и ждёт, пока тот выключен, поэтому
// обработчик уничтоженного компонента не вызывается никогда.
//
// Наступившие за кадр таймеры срабатывают по времени наступления, при равном -
// по порядку создания. Повторяющийся с интервалом короче кадра срабатывает по
// разу на каждый прошедший интервал. Таймер, заведённый в обработчике, -
// не раньше следующего кадра. Исключение обработчика пишется в лог.
class YQLORA_API TimerSystem final : public SceneSystem {
public:
    YQLORA_TYPE(TimerSystem, SceneSystem)

    // Не встроенные: таймер (Timer) описан только в .cpp.
    explicit TimerSystem(Scene& scene);
    ~TimerSystem() override;

    using Callback = std::function<void()>;

    struct Options {
        // Повтор через interval; 0 - один раз.
        float interval = 0.0f;
        bool unscaled = false;
    };

    // Через seconds секунд (0 - в ближайшем update), с повтором, если задан.
    TimerHandle start(Component& owner, float seconds, Callback callback, Options options);

    void cancel(TimerHandle timer);
    void pause(TimerHandle timer);
    void resume(TimerHandle timer);

    // Секунд до срабатывания; nullopt - таймера нет (сработал, снят).
    std::optional<float> remaining(TimerHandle timer) const;

    bool active(TimerHandle timer) const;

    void update(float deltaSeconds) override;

private:
    struct Timer;

    Timer* find(TimerHandle timer) const;

    std::vector<std::unique_ptr<Timer>> m_timers;
    std::uint64_t m_nextId = 1;
};

// Таймеры от имени компонента: Script::timers().after(2.0f, [this] { ... }).
class YQLORA_API Timers {
public:
    Timers(TimerSystem& system, Component& owner)
        : m_system(system)
        , m_owner(owner)
    {
    }

    TimerHandle after(float seconds, TimerSystem::Callback callback, bool unscaled = false);
    TimerHandle every(float seconds, TimerSystem::Callback callback, bool unscaled = false);

    void cancel(TimerHandle timer)
    {
        m_system.cancel(timer);
    }

    void pause(TimerHandle timer)
    {
        m_system.pause(timer);
    }

    void resume(TimerHandle timer)
    {
        m_system.resume(timer);
    }

    std::optional<float> remaining(TimerHandle timer) const
    {
        return m_system.remaining(timer);
    }

private:
    TimerSystem& m_system;
    Component& m_owner;
};

} // namespace yqlora
