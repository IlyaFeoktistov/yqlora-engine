#pragma once

#include "yqlora/core/Export.hpp"

#include <string_view>
#include <vector>

namespace yqlora {

class Component;
class Node;
class TypeRegistry;

// Компоненты узла по имени типа в реестре: так их находят Luau, визуальные
// скрипты и типы, объявленные во время выполнения, у которых нет своего
// C++-класса. Тип подходит, если это typeName или его наследник.

// Первый подходящий компонент узла; nullptr, если такого нет.
YQLORA_API Component* componentOfType(const Node& node, std::string_view typeName, const TypeRegistry& registry);

// Все подходящие компоненты узла в его порядке.
YQLORA_API std::vector<Component*> componentsOfType(
    const Node& node,
    std::string_view typeName,
    const TypeRegistry& registry
);

} // namespace yqlora
