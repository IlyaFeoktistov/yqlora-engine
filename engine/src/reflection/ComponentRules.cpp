#include "yqlora/reflection/ComponentRules.hpp"

#include "yqlora/reflection/TypeRegistry.hpp"
#include "yqlora/scene/Node.hpp"

#include <algorithm>

namespace yqlora {

namespace {

std::vector<std::string_view> requirementsOf(const TypeRegistry& registry, const TypeInfo& type)
{
    std::vector<std::string_view> result;
    for (const TypeInfo* t : registry.chain(type)) {
        for (const std::string& required : t->requiredComponents()) {
            result.emplace_back(required);
        }
    }

    return result;
}

// Тип узла, которого требует type или его база и которому node не
// подходит; пусто - узел подходит.
std::string_view unfitNodeType(const TypeRegistry& registry, const TypeInfo& type, const Node& node)
{
    const TypeInfo* nodeType = registry.find(node.typeName());
    for (const TypeInfo* t : registry.chain(type)) {
        const std::string& required = t->requiredNode();
        if (!required.empty() && (!nodeType || !derivesFrom(registry, *nodeType, required))) {
            return required;
        }
    }

    return nodeType ? std::string_view() : std::string_view(node.typeName());
}

// На node есть компонент типа required (или наследника), кроме except.
bool hasComponentOf(
    const TypeRegistry& registry,
    const Node& node,
    std::string_view required,
    const Component* except = nullptr
)
{
    return std::ranges::any_of(node.components(), [&](const auto& component) {
        const TypeInfo* type = registry.find(component->typeName());

        return component.get() != except && type && derivesFrom(registry, *type, required);
    });
}

// adding - типы, которые уже решено добавить (и сам исходный тип): они
// закрывают требования так же, как компоненты на узле, и обрывают циклы.
// false - обязательный компонент unavailable не добавить.
bool collectMissing(
    const TypeRegistry& registry,
    const TypeInfo& type,
    const Node& node,
    std::vector<const TypeInfo*>& adding,
    std::vector<const TypeInfo*>& result,
    std::string_view& unavailable
)
{
    for (const std::string_view required : requirementsOf(registry, type)) {
        const bool planned = std::ranges::any_of(adding, [&](const TypeInfo* t) {
            return derivesFrom(registry, *t, required);
        });
        if (planned || hasComponentOf(registry, node, required)) {
            continue;
        }

        const TypeInfo* requiredType = registry.find(required);
        if (!requiredType || requiredType->kind() != TypeKind::Component || !requiredType->creatable()
            || !canAttach(registry, *requiredType, node)) {
            unavailable = required;

            return false;
        }
        adding.push_back(requiredType);
        if (!collectMissing(registry, *requiredType, node, adding, result, unavailable)) {
            return false;
        }
        result.push_back(requiredType);
    }

    return true;
}

} // namespace

bool derivesFrom(const TypeRegistry& registry, const TypeInfo& type, std::string_view base)
{
    return std::ranges::any_of(registry.chain(type), [&](const TypeInfo* t) {
        return t->name() == base;
    });
}

bool canAttach(const TypeRegistry& registry, const TypeInfo& type, const Node& node)
{
    if (type.kind() != TypeKind::Component || !unfitNodeType(registry, type, node).empty()) {
        return false;
    }
    const bool multiple = std::ranges::any_of(registry.chain(type), [](const TypeInfo* t) {
        return t->allowsMultiple();
    });

    return multiple || std::ranges::none_of(node.components(), [&](const auto& component) {
               return component->typeName() == type.name();
           });
}

AttachCheck checkAttach(const TypeRegistry& registry, const TypeInfo& type, const Node& node)
{
    if (const std::string_view nodeType = unfitNodeType(registry, type, node); !nodeType.empty()) {
        return {AttachProblem::WrongNode, std::string(nodeType)};
    }
    if (!canAttach(registry, type, node)) {
        return {AttachProblem::AlreadyAttached, {}};
    }
    std::vector<const TypeInfo*> adding{&type};
    std::vector<const TypeInfo*> missing;
    std::string_view unavailable;
    if (!collectMissing(registry, type, node, adding, missing, unavailable)) {
        return {AttachProblem::RequirementUnavailable, std::string(unavailable)};
    }

    return {};
}

std::optional<std::vector<const TypeInfo*>> missingRequirements(
    const TypeRegistry& registry,
    const TypeInfo& type,
    const Node& node
)
{
    std::vector<const TypeInfo*> adding{&type};
    std::vector<const TypeInfo*> result;
    std::string_view unavailable;
    if (!collectMissing(registry, type, node, adding, result, unavailable)) {
        return std::nullopt;
    }

    return result;
}

std::vector<const Component*> dependentsOf(const TypeRegistry& registry, const Node& node, const Component& component)
{
    std::vector<const Component*> result;
    const TypeInfo* removedType = registry.find(component.typeName());
    if (!removedType) {
        return result;
    }

    for (const auto& other : node.components()) {
        const TypeInfo* otherType = registry.find(other->typeName());
        if (other.get() == &component || !otherType) {
            continue;
        }
        const auto requirements = requirementsOf(registry, *otherType);
        const bool loses = std::ranges::any_of(requirements, [&](std::string_view required) {
            return derivesFrom(registry, *removedType, required)
                   && !hasComponentOf(registry, node, required, &component);
        });
        if (loses) {
            result.push_back(other.get());
        }
    }

    return result;
}

bool canDetach(const TypeRegistry& registry, const Node& node, const Component& component)
{
    return node.canRemoveComponent(component) && dependentsOf(registry, node, component).empty();
}

std::vector<const TypeInfo*> attachableTypes(const TypeRegistry& registry, const Node& node)
{
    std::vector<const TypeInfo*> result;
    for (const auto& type : registry.types()) {
        if (type->creatable() && type->kind() == TypeKind::Component
            && checkAttach(registry, *type, node).problem == AttachProblem::None) {
            result.push_back(type.get());
        }
    }

    return result;
}

} // namespace yqlora
