#pragma once

#include "yqlora/core/Export.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace yqlora {

class Component;
class Node;
class TypeInfo;
class TypeRegistry;

// Правила компонентов из реестра (requiresNode, requiresComponent, allowMultiple),
// см. BasicTypeBuilder. Правила базового типа действуют и на наследников.

// type - это тип base или его наследник.
YQLORA_API bool derivesFrom(const TypeRegistry& registry, const TypeInfo& type, std::string_view base);

// Компонент типа type можно подключить к node: узел подходит по requiresNode,
// и компонента ровно этого типа на нём ещё нет, если тип не allowMultiple.
// Обязательные компоненты здесь не проверяются, см. missingRequirements.
YQLORA_API bool canAttach(const TypeRegistry& registry, const TypeInfo& type, const Node& node);

// Каких обязательных компонентов не хватает на node, чтобы подключить type,
// вместе с обязательными для них самих. Порядок - в каком добавлять: сначала
// те, от которых зависят. nullopt, если какой-то из них не зарегистрирован,
// не создаётся или не подходит узлу.
YQLORA_API std::optional<std::vector<const TypeInfo*>> missingRequirements(
    const TypeRegistry& registry,
    const TypeInfo& type,
    const Node& node
);

// Компоненты node, которые останутся без обязательного, если снять component.
YQLORA_API std::vector<const Component*> dependentsOf(
    const TypeRegistry& registry,
    const Node& node,
    const Component& component
);

// Компонент можно снять с узла: он не встроенный и не нужен другим (dependentsOf).
YQLORA_API bool canDetach(const TypeRegistry& registry, const Node& node, const Component& component);

// Почему компонент не добавить к узлу; None - можно (вместе с недостающими
// обязательными).
enum class AttachProblem : std::uint8_t {
    None,
    // Узел не того типа: компонент живёт только на AttachCheck::type.
    WrongNode,
    // Компонент ровно этого типа уже на узле, а несколько нельзя.
    AlreadyAttached,
    // Обязательный компонент AttachCheck::type не добавить: он не подходит
    // узлу, не создаётся или не зарегистрирован.
    RequirementUnavailable,
};

struct AttachCheck {
    AttachProblem problem = AttachProblem::None;
    // Нужный тип узла (WrongNode) или обязательный компонент
    // (RequirementUnavailable); пусто у остальных.
    std::string type;
};

// Можно ли добавить компонент типа type к node, а если нет - почему.
// Окно выбора типа показывает неподходящий тип с этой причиной.
YQLORA_API AttachCheck checkAttach(const TypeRegistry& registry, const TypeInfo& type, const Node& node);

// Типы компонентов, которые можно добавить к node вместе с недостающими
// обязательными, в порядке регистрации.
YQLORA_API std::vector<const TypeInfo*> attachableTypes(const TypeRegistry& registry, const Node& node);

} // namespace yqlora
