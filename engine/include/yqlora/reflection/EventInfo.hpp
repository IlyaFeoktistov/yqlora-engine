#pragma once

#include "yqlora/reflection/Property.hpp"

#include <functional>
#include <string>
#include <vector>

namespace yqlora {

class Component;
class EventBase;

// Параметр события: имя для подписи и тип значения, которым его получают
// Luau, визуальные скрипты и постоянные подписчики.
struct EventParameter {
    std::string name;
    PropertyType type = PropertyType::Bool;
};

// Событие компонента в реестре: .event("onTimeout", &Timer::onTimeout).
struct EventInfo {
    std::string name;
    std::vector<EventParameter> parameters;
    // Само событие у компонента этого типа или наследника.
    std::function<EventBase&(Component&)> of;
};

} // namespace yqlora
