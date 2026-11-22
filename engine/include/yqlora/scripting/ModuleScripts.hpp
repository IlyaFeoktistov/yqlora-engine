#pragma once

#include "yqlora/scripting/ModuleRegistrar.hpp"

#include <algorithm>
#include <concepts>
#include <functional>
#include <initializer_list>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace yqlora {

namespace detail {

// Скрипт модуля, который ждёт входной функции: регистрирует себя в ней.
class ModuleScript {
public:
    virtual ~ModuleScript() = default;

    virtual std::string_view name() const = 0;
    virtual void registerIn(ModuleRegistrar& registrar) const = 0;
};

// Скрипты этого модуля. Своя копия в каждой библиотеке игры: функция inline,
// а символы модуля скрыты. Список - статическая переменная функции, поэтому
// порядок статической инициализации файлов ему не важен.
inline std::vector<std::unique_ptr<ModuleScript>>& moduleScripts()
{
    static std::vector<std::unique_ptr<ModuleScript>> scripts;

    return scripts;
}

} // namespace detail

// Регистрация скрипта, записанная при загрузке библиотеки и выполненная во
// входной функции модуля. Правила - те же, что у ScriptBuilder.
template <std::derived_from<Script> T>
class ScriptRegistration final : public detail::ModuleScript {
public:
    explicit ScriptRegistration(std::string sourceFile)
        : m_sourceFile(std::move(sourceFile))
    {
    }

    ScriptRegistration& description(std::string text)
    {
        m_rules.push_back([text = std::move(text)](ScriptBuilder<T>& builder) {
            builder.description(text);
        });

        return *this;
    }

    ScriptRegistration& category(std::string path)
    {
        m_rules.push_back([path = std::move(path)](ScriptBuilder<T>& builder) {
            builder.category(path);
        });

        return *this;
    }

    ScriptRegistration& formerly(std::string oldName)
    {
        m_rules.push_back([oldName = std::move(oldName)](ScriptBuilder<T>& builder) {
            builder.formerly(oldName);
        });

        return *this;
    }

    template <std::derived_from<Node> NodeType>
    ScriptRegistration& requiresNode()
    {
        m_rules.push_back([](ScriptBuilder<T>& builder) {
            builder.template requiresNode<NodeType>();
        });

        return *this;
    }

    template <std::derived_from<Component> Required>
    ScriptRegistration& requiresComponent()
    {
        m_rules.push_back([](ScriptBuilder<T>& builder) {
            builder.template requiresComponent<Required>();
        });

        return *this;
    }

    ScriptRegistration& executionOrder(int order)
    {
        m_rules.push_back([order](ScriptBuilder<T>& builder) {
            builder.executionOrder(order);
        });

        return *this;
    }

    template <typename Function>
    ScriptRegistration& method(
        std::string name,
        Function function,
        std::initializer_list<ClassMethodParameter> parameters = {}
    )
    {
        // Регистрация выполняется позже, во входной функции: список хранится копией.
        m_rules.push_back(
            [name = std::move(name), function, parameters = std::vector(parameters)](ScriptBuilder<T>& builder) {
                builder.method(name, function, std::span<const ClassMethodParameter>(parameters));
            }
        );

        return *this;
    }

    template <typename Member>
    ScriptRegistration& event(std::string name, Member member, std::initializer_list<std::string> parameterNames = {})
    {
        m_rules.push_back(
            [name = std::move(name), member, parameterNames = std::vector(parameterNames)](ScriptBuilder<T>& builder) {
                builder.event(name, member, std::span<const std::string>(parameterNames));
            }
        );

        return *this;
    }

    std::string_view name() const override
    {
        return typeNameOf<T>();
    }

    void registerIn(ModuleRegistrar& registrar) const override
    {
        ScriptBuilder<T> builder = registrar.registerScript<T>();
        builder.sourceFile(m_sourceFile);
        for (const auto& rule : m_rules) {
            rule(builder);
        }
    }

private:
    std::string m_sourceFile;
    std::vector<std::function<void(ScriptBuilder<T>&)>> m_rules;
};

// sourceFile - файл, в котором скрипт зарегистрирован (__FILE__).
template <std::derived_from<Script> T>
ScriptRegistration<T>& addModuleScript(std::string sourceFile = {})
{
    auto& scripts = detail::moduleScripts();
    scripts.push_back(std::make_unique<ScriptRegistration<T>>(std::move(sourceFile)));

    return static_cast<ScriptRegistration<T>&>(*scripts.back());
}

// Регистрирует все скрипты модуля по имени: порядок файлов в сборке на
// реестр не влияет.
inline void registerModuleScripts(ModuleRegistrar& registrar)
{
    std::vector<const detail::ModuleScript*> sorted;
    for (const auto& script : detail::moduleScripts()) {
        sorted.push_back(script.get());
    }
    std::ranges::sort(sorted, {}, &detail::ModuleScript::name);
    for (const detail::ModuleScript* script : sorted) {
        script->registerIn(registrar);
    }
}

} // namespace yqlora

#define YQLORA_DETAIL_CONCAT_INNER(a, b) a##b
#define YQLORA_DETAIL_CONCAT(a, b) YQLORA_DETAIL_CONCAT_INNER(a, b)

// Регистрирует скрипт в модуле игры; ставится в его .cpp, один раз:
//
//   YQLORA_REGISTER_SCRIPT(Spinner).requiresNode<yqlora::Node3D>();
//
// Правила после него - как у ScriptBuilder: description, category,
// formerly, requiresNode, requiresComponent, executionOrder, method, event.
// Входную функцию модуля даёт SDK.
#define YQLORA_REGISTER_SCRIPT(Type)                                                                                   \
    [[maybe_unused]] static auto& YQLORA_DETAIL_CONCAT(                                                                \
        yqloraScriptRegistration,                                                                                      \
        __LINE__                                                                                                       \
    ) = ::yqlora::addModuleScript<Type>(__FILE__)
