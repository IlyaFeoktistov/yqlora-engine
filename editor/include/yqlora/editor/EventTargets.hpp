#pragma once

#include "yqlora/reflection/EventInfo.hpp"

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace yqlora {
class Component;
class Method;
class TypeRegistry;
} // namespace yqlora

namespace yqlora::editor {

// Что предлагает поле постоянных подписчиков события (PropertyHint::EventTargets).

// Параметры события eventName у компонента; пусто, если такого события нет.
std::span<const EventParameter> eventParametersOf(
    const Component& component,
    std::string_view eventName,
    const TypeRegistry& registry
);

// Метод слушает событие: без результата (подписчик - действие, а не запрос)
// и без параметров (слушает любое) или с теми же типами параметров по порядку.
bool methodFitsEvent(const Method& method, std::span<const EventParameter> parameters);

// Имена методов типа typeName (с базовыми), которые слушают событие с этими
// параметрами, в порядке реестра.
std::vector<std::string> eventMethods(
    std::string_view typeName,
    std::span<const EventParameter> parameters,
    const TypeRegistry& registry
);

} // namespace yqlora::editor
