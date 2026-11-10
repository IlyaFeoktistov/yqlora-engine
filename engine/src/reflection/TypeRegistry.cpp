#include "yqlora/reflection/TypeRegistry.hpp"

#include <algorithm>
#include <cassert>
#include <ranges>

namespace yqlora {

TypeBuilder TypeRegistry::registerType(std::string name, TypeKind kind, std::string base)
{
    return TypeBuilder(addType(std::move(name), kind, std::move(base)));
}

TypeInfo& TypeRegistry::addType(std::string name, TypeKind kind, std::string base)
{
    assert(!find(name) && "TypeRegistry: type name is already registered");
    if (!base.empty()) {
        [[maybe_unused]] const TypeInfo* baseType = find(base);
        assert(baseType && "TypeRegistry: register the base type first");
        assert(baseType->kind() == kind && "TypeRegistry: the base is a type of another kind");
    }

    auto info = std::make_unique<TypeInfo>();
    info->m_name = std::move(name);
    info->m_baseName = std::move(base);
    info->m_module = m_module;
    info->m_kind = kind;

    TypeInfo& ref = *info;
    m_byName.emplace(ref.m_name, &ref);
    m_types.push_back(std::move(info));
    ++m_revision;

    return ref;
}

const TypeInfo* TypeRegistry::find(std::string_view name) const
{
    if (const auto it = m_byName.find(name); it != m_byName.end()) {
        return it->second;
    }
    const auto renamed = std::ranges::find_if(m_types, [&](const std::unique_ptr<TypeInfo>& type) {
        return std::ranges::find(type->formerNames(), name) != type->formerNames().end();
    });

    return renamed != m_types.end() ? renamed->get() : nullptr;
}

std::vector<const TypeInfo*> TypeRegistry::chain(const TypeInfo& type) const
{
    std::vector<const TypeInfo*> result;
    for (const TypeInfo* t = &type; t; t = t->baseName().empty() ? nullptr : find(t->baseName())) {
        result.push_back(t);
    }

    return result;
}

std::vector<const Property*> TypeRegistry::properties(const TypeInfo& type) const
{
    // Базовые свойства первыми.
    std::vector<const Property*> result;
    for (const TypeInfo* t : chain(type) | std::views::reverse) {
        if (type.builtin() && t->baseName().empty()) {
            continue;
        }
        for (const Property& property : t->ownProperties()) {
            result.push_back(&property);
        }
    }

    return result;
}

std::vector<const Method*> TypeRegistry::methods(const TypeInfo& type) const
{
    std::vector<const Method*> result;
    for (const TypeInfo* t : chain(type) | std::views::reverse) {
        for (const Method& method : t->ownMethods()) {
            result.push_back(&method);
        }
    }

    return result;
}

std::vector<const EventInfo*> TypeRegistry::events(const TypeInfo& type) const
{
    std::vector<const EventInfo*> result;
    for (const TypeInfo* t : chain(type) | std::views::reverse) {
        for (const EventInfo& event : t->ownEvents()) {
            result.push_back(&event);
        }
    }

    return result;
}

std::size_t TypeRegistry::removeModule(std::string_view module)
{
    const auto inModule = [&](const std::unique_ptr<TypeInfo>& type) {
        return type->module() == module;
    };
#ifndef NDEBUG
    for (const auto& type : m_types) {
        if (!inModule(type) && !type->baseName().empty()) {
            const TypeInfo* base = find(type->baseName());
            assert((!base || base->module() != module) && "removeModule: a type of another module derives from it");
        }
    }
#endif

    std::erase_if(m_byName, [&](const auto& entry) {
        return entry.second->module() == module;
    });

    ++m_revision;

    return std::erase_if(m_types, inModule);
}

} // namespace yqlora
