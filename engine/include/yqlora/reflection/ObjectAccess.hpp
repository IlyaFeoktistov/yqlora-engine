#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/reflection/Method.hpp"
#include "yqlora/reflection/Property.hpp"

#include <optional>
#include <span>
#include <string_view>

namespace yqlora {

class Component;
class Node;
class TypeRegistry;

// Свойства и методы узла или компонента по имени - те же записи реестра, что
// показывает инспектор, со свойствами базовых типов. Так к объектам
// обращаются Luau и визуальные скрипты; код на C++ зовёт сеттеры напрямую.

// nullopt, если у типа объекта нет такого свойства.
YQLORA_API std::optional<PropertyValue> getProperty(
    const Node& node,
    std::string_view name,
    const TypeRegistry& registry
);
YQLORA_API std::optional<PropertyValue> getProperty(
    const Component& component,
    std::string_view name,
    const TypeRegistry& registry
);

// false, если свойства нет, значение другого типа или свойство только для чтения.
YQLORA_API bool setProperty(
    Node& node,
    std::string_view name,
    const PropertyValue& value,
    const TypeRegistry& registry
);
YQLORA_API bool setProperty(
    Component& component,
    std::string_view name,
    const PropertyValue& value,
    const TypeRegistry& registry
);

// called false, если метода нет или аргументы не подошли.
YQLORA_API MethodCallResult
callMethod(Node& node, std::string_view name, std::span<const PropertyValue> arguments, const TypeRegistry& registry);
YQLORA_API MethodCallResult callMethod(
    Component& component,
    std::string_view name,
    std::span<const PropertyValue> arguments,
    const TypeRegistry& registry
);

} // namespace yqlora
