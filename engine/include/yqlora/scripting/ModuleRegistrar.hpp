#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/reflection/TypeRegistry.hpp"
#include "yqlora/scripting/Script.hpp"

#include <concepts>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace yqlora {

class ModuleRegistrar;

// Входная функция модуля игры: в ней библиотека регистрирует свои типы.
// Модули через yqlora_add_game_module получают её готовой
// (cmake/GameModuleEntry.cpp): она регистрирует скрипты, отмеченные
// YQLORA_REGISTER_SCRIPT (ModuleScripts.hpp).
//
//   YQLORA_MODULE_ENTRY(registrar)
//   {
//       registrar.registerScript<Player>();
//   }
#define YQLORA_MODULE_ENTRY(registrar)                                                                                 \
    extern "C" YQLORA_MODULE_EXPORT void YQLORA_MODULE_ENTRY_NAME(::yqlora::ModuleRegistrar& registrar)

// Имя входной функции: одно и для макроса, и для поиска в библиотеке (kModuleEntrySymbol).
#define YQLORA_MODULE_ENTRY_NAME yqloraModuleEntry
#define YQLORA_DETAIL_STRING(text) #text
#define YQLORA_DETAIL_EXPANDED_STRING(macro) YQLORA_DETAIL_STRING(macro)

using ModuleEntry = void (*)(ModuleRegistrar&);

inline constexpr const char* kModuleEntrySymbol = YQLORA_DETAIL_EXPANDED_STRING(YQLORA_MODULE_ENTRY_NAME);

// Регистрация, которую реестр не принял: имя уже занято типом модуля takenBy.
struct RejectedType {
    std::string name;
    std::string takenBy;
};

// Правила типа скрипта сверх его полей (поля - в YQLORA_FIELDS). У отклонённого
// скрипта цепочка ничего не делает.
template <std::derived_from<Script> T>
class ScriptBuilder {
public:
    ScriptBuilder() = default;

    explicit ScriptBuilder(ClassTypeBuilder<T, Component> builder)
        : m_builder(builder)
    {
    }

    // Класс переименован: файлы со старым именем типа читаются, пишется новое.
    ScriptBuilder& formerly(std::string oldName)
    {
        if (m_builder) {
            m_builder->formerTypeName(std::move(oldName));
        }

        return *this;
    }

    template <std::derived_from<Node> NodeType>
    ScriptBuilder& requiresNode()
    {
        if (m_builder) {
            m_builder->template requiresNode<NodeType>();
        }

        return *this;
    }

    template <std::derived_from<Component> Required>
    ScriptBuilder& requiresComponent()
    {
        if (m_builder) {
            m_builder->template requiresComponent<Required>();
        }

        return *this;
    }

    ScriptBuilder& executionOrder(int order)
    {
        if (m_builder) {
            m_builder->executionOrder(order);
        }

        return *this;
    }

    // Метод скрипта для постоянных подписчиков событий, Luau и визуальных
    // скриптов: как ClassTypeBuilder::method, имя на каждый параметр.
    //   .method("open", &Door::open)
    template <typename Function>
    ScriptBuilder& method(std::string name, Function function, std::span<const ClassMethodParameter> parameters)
    {
        if (m_builder) {
            m_builder->method(std::move(name), function, parameters);
        }

        return *this;
    }

    template <typename Function>
    ScriptBuilder& method(
        std::string name,
        Function function,
        std::initializer_list<ClassMethodParameter> parameters = {}
    )
    {
        return method(std::move(name), function, std::span(parameters.begin(), parameters.size()));
    }

    // Событие скрипта, как ClassTypeBuilder::event: имя на каждый аргумент.
    //   .event("onOpened", &Door::onOpened)
    template <typename Member>
    ScriptBuilder& event(std::string name, Member member, std::span<const std::string> parameterNames)
    {
        if (m_builder) {
            m_builder->event(std::move(name), member, parameterNames);
        }

        return *this;
    }

    template <typename Member>
    ScriptBuilder& event(std::string name, Member member, std::initializer_list<std::string> parameterNames = {})
    {
        return event(std::move(name), member, std::span(parameterNames.begin(), parameterNames.size()));
    }

private:
    std::optional<ClassTypeBuilder<T, Component>> m_builder;
};

// То, что входная функция модуля получает от движка: регистрация типов на
// модуль module. Имя, уже занятое любым типом (своего или чужого модуля, в
// том числе прежнее имя), не регистрируется, а попадает в rejected(), чтобы
// загрузивший модуль сообщил об этом, не роняя редактор.
class YQLORA_API ModuleRegistrar {
public:
    ModuleRegistrar(TypeRegistry& registry, std::string module);

    ModuleRegistrar(const ModuleRegistrar&) = delete;
    ModuleRegistrar& operator=(const ModuleRegistrar&) = delete;

    // Скрипт на C++: тип с пометкой "скрипт", несколько на узле, и его поля из
    // YQLORA_FIELDS со всеми атрибутами.
    template <std::derived_from<Script> T>
    ScriptBuilder<T> registerScript()
    {
        if (!accept(typeNameOf<T>())) {
            return ScriptBuilder<T>();
        }
        auto builder = m_registry.registerComponent<T>();
        builder.script().allowMultiple();
        std::apply(
            [&](const auto&... fields) {
                (addField(builder, fields), ...);
            },
            ScriptFields::of<T>()
        );

        return ScriptBuilder<T>(builder);
    }

    std::span<const RejectedType> rejected() const
    {
        return m_rejected;
    }

private:
    // false и запись в rejected, если имя занято.
    bool accept(std::string_view name);

    template <typename Builder, typename Field>
    static void addField(Builder& builder, const Field& field)
    {
        builder.property(std::string(field.name), field.member, field.hint);
        builder.withAttributes(field.attributes);
    }

    TypeRegistry& m_registry;
    TypeRegistry::ModuleScope m_scope;
    std::vector<RejectedType> m_rejected;
};

} // namespace yqlora
