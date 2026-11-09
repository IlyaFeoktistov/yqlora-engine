#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/reflection/TypeName.hpp"
#include "yqlora/scene/Component.hpp"
#include "yqlora/scene/Event.hpp"
#include "yqlora/scene/TimerSystem.hpp"

namespace yqlora {

// Таймер, заданный в инспекторе: через duration секунд (и каждые duration,
// если repeat) - событие onTimeout. Таймер TimerSystem, которым владеет сам
// компонент: снимается с ним, ждёт, пока он выключен. autostart - запуск в
// начале игры (onStart), поэтому в редактируемой сцене таймер не идёт.
// Идущий таймер не сохраняется: перезагрузка начинает со свойств.
class YQLORA_API Timer final : public Component {
public:
    YQLORA_TYPE(Timer, Component)

    float duration = 1.0f;
    bool repeat = false;
    bool autostart = false;
    // По настоящему времени: идёт и во время паузы (интерфейс).
    bool unscaled = false;

    Event<> onTimeout;

    // Запускает заново с полным duration; идущий таймер перезапускается.
    void start();
    void stop();

    bool running() const;

    // Секунд до срабатывания; 0, если таймер не идёт.
    float remaining() const;

    void onStart() override;
    void onStop() override;

private:
    TimerSystem* system() const;

    TimerHandle m_handle;
};

} // namespace yqlora
