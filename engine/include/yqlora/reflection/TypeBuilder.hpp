#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/project/SettingsSection.hpp"
#include "yqlora/reflection/Property.hpp"
#include "yqlora/reflection/Record.hpp"
#include "yqlora/reflection/TypeInfo.hpp"
#include "yqlora/reflection/TypeName.hpp"
#include "yqlora/scene/Component.hpp"
#include "yqlora/scene/Event.hpp"
#include "yqlora/scene/Node.hpp"
#include "yqlora/scene/SceneSystem.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace yqlora {

// Описание типа, общее для любого способа регистрации: свойства значениями и
// правила компонента. Self - конкретный строитель: методы возвращают его,
// чтобы в цепочке вызовов не терялись его собственные методы.
template <typename Self>
class BasicTypeBuilder : public PropertyAttributeSetters<Self> {
public:
    // Свойство, уже описанное значениями (Property::forNode, forComponent, forSettings).
    Self& property(Property property)
    {
        m_info.m_properties.push_back(std::move(property));

        return self();
    }

    // Событие, уже описанное значениями (см. ClassTypeBuilder::event).
    Self& addEvent(EventInfo event)
    {
        assert(
            std::ranges::none_of(
                m_info.m_events,
                [&](const EventInfo& e) {
                    return e.name == event.name;
                }
            )
            && "TypeBuilder: the type already has an event with this name"
        );
        m_info.m_events.push_back(std::move(event));

        return self();
    }

    // Метод, уже описанный значениями (Method::forNode, forComponent).
    Self& method(Method method)
    {
        assert(
            std::ranges::none_of(
                m_info.m_methods,
                [&](const Method& m) {
                    return m.name() == method.name();
                }
            )
            && "TypeBuilder: the type already has a method with this name"
        );
        m_info.m_methods.push_back(std::move(method));

        return self();
    }

    // Подписи элементов последнего свойства-списка компонента, см.
    // Property::ElementLabels.
    Self& elementLabels(Property::ElementLabels labels)
    {
        assertComponent();
        lastProperty().setElementLabels(std::move(labels));

        return self();
    }

    // Тип переименован: файлы со старым именем читаются, пишется новое. Не
    // formerly: так в цепочке называется прежнее имя последнего свойства.
    Self& formerTypeName(std::string oldName)
    {
        m_info.m_formerNames.push_back(std::move(oldName));

        return self();
    }

    // Компонент живёт только на узлах типа nodeType и его наследников.
    Self& requiresNode(std::string nodeType)
    {
        assertComponent();
        m_info.m_requiredNode = std::move(nodeType);

        return self();
    }

    // На том же узле нужен компонент типа componentType (или его наследника).
    // Редактор добавляет его сам, а снять его нельзя, пока от него зависят.
    Self& requiresComponent(std::string componentType)
    {
        assertComponent();
        m_info.m_requiredComponents.push_back(std::move(componentType));

        return self();
    }

    // Порядок выполнения в фазах кадра по умолчанию (меньше - раньше, по
    // умолчанию 0). Настройки проекта могут его переопределить, см. executionOrderOf.
    Self& executionOrder(int order)
    {
        assertComponent();
        m_info.m_executionOrder = order;

        return self();
    }

    // Тип - скрипт (см. TypeInfo::isScript). Ставит каждый язык скриптов.
    Self& script()
    {
        assertComponent();
        m_info.m_script = true;

        return self();
    }

    // Все атрибуты последнего добавленного свойства разом: из описания поля.
    // То, что вывел тип C++ свойства (вид ресурса Ref<T>, тип компонента
    // ComponentRef<T>), остаётся, если в attributes оно не задано.
    Self& withAttributes(PropertyAttributes attributes)
    {
        attributesToEdit() = withTypeAttributes(std::move(attributes), attributesToEdit());

        return self();
    }

    // Несколько компонентов этого типа на одном узле. По умолчанию один.
    Self& allowMultiple()
    {
        assertComponent();
        m_info.m_allowMultiple = true;

        return self();
    }

protected:
    explicit BasicTypeBuilder(TypeInfo& info)
        : m_info(info)
    {
    }

    Self& self()
    {
        return static_cast<Self&>(*this);
    }

    // Атрибуты (PropertyAttributeSetters) относятся к последнему добавленному свойству.
    friend class PropertyAttributeSetters<Self>;

    PropertyAttributes& attributesToEdit()
    {
        return lastProperty().attributes();
    }

    Property& lastProperty()
    {
        assert(!m_info.m_properties.empty() && "TypeBuilder: add the property first");

        return m_info.m_properties.back();
    }

    void assertComponent() const
    {
        assert(m_info.m_kind == TypeKind::Component && "TypeBuilder: this rule is for components only");
    }

    TypeInfo& m_info;
};

// Тип, описанный во время выполнения, без своего C++-класса: скрипт Luau,
// граф визуального скрипта. Создание и фазы задаются явно:
//   registry.registerType("Player", TypeKind::Component, "Script")
//       .createComponent([script] { return std::make_unique<LuauScript>(script); })
//       .phases(phaseBit(LifecyclePhase::Update))
//       .property(Property::forComponent("speed", PropertyType::Float, getSpeed, setSpeed));
class YQLORA_API TypeBuilder final : public BasicTypeBuilder<TypeBuilder> {
public:
    explicit TypeBuilder(TypeInfo& info)
        : BasicTypeBuilder(info)
    {
    }

    TypeBuilder& createNode(std::function<std::unique_ptr<Node>()> factory);
    TypeBuilder& createComponent(std::function<std::unique_ptr<Component>()> factory);
    TypeBuilder& createSystem(std::function<std::unique_ptr<SceneSystem>(Scene&)> factory);

    // Фазы кадра, в которых сцена вызывает компоненты типа.
    TypeBuilder& phases(LifecyclePhases phases);
};

// Параметр метода C++-класса: тип берётся из сигнатуры, здесь только имя и,
// если нужно, значение по умолчанию.
struct ClassMethodParameter {
    ClassMethodParameter(std::string parameterName, std::optional<PropertyValue> value = std::nullopt)
        : name(std::move(parameterName))
        , defaultValue(std::move(value))
    {
    }

    std::string name;
    std::optional<PropertyValue> defaultValue;
};

// Тип - C++-класс: свойства из его методов и полей, правила по C++-типам.
// Сам ничего не хранит, переводит члены класса в значения BasicTypeBuilder:
//   registry.registerNode<Camera3D>()
//       .property("fov", &Camera3D::fovDegrees, &Camera3D::setFovDegrees);
template <typename Class, typename Root>
class ClassTypeBuilder final : public BasicTypeBuilder<ClassTypeBuilder<Class, Root>> {
    using Base = BasicTypeBuilder<ClassTypeBuilder<Class, Root>>;

public:
    explicit ClassTypeBuilder(TypeInfo& info)
        : Base(info)
    {
    }

    using Base::method;
    using Base::property;
    using Base::requiresComponent;
    using Base::requiresNode;

    // Свойство через getter и setter: методы класса или любые вызываемые объекты
    // вида Value(const Class&) и void(Class&, Value).
    template <typename Getter, typename Setter>
    ClassTypeBuilder& property(std::string name, Getter getter, Setter setter, PropertyHint hint = PropertyHint::None)
        requires(!std::same_as<Root, SceneSystem>)
    {
        using Value = std::remove_cvref_t<std::invoke_result_t<Getter, const Class&>>;
        using Storage = PropertyStorage<Value>;
        using Stored = typename Storage::Type;
        const PropertyType type = propertyTypeOf<Value>();

        const auto get = [getter](const Root& object) -> PropertyValue {
            return PropertyValue(
                std::in_place_type<Stored>,
                Storage::store(std::invoke(getter, static_cast<const Class&>(object)))
            );
        };
        const auto set = [setter](Root& object, const PropertyValue& value) {
            std::invoke(setter, static_cast<Class&>(object), Storage::load(std::get<Stored>(value)));
        };
        Property built = [&] {
            if constexpr (std::same_as<Root, Node>) {
                return Property::forNode(std::move(name), type, get, set, hint);
            } else if constexpr (std::same_as<Root, Component>) {
                return Property::forComponent(std::move(name), type, get, set, hint);
            } else {
                return Property::forSettings(std::move(name), type, get, set, hint);
            }
        }();
        describeValue<Value>(built);

        return property(std::move(built));
    }

    // Свойство-поле, когда getter и setter не нужны: .property("radius", &SphereShape::radius);
    template <typename Value>
    ClassTypeBuilder& property(std::string name, Value Class::* field, PropertyHint hint = PropertyHint::None)
        requires(!std::same_as<Root, SceneSystem>)
    {
        const auto getter = [field](const Class& object) -> const Value& {
            return object.*field;
        };
        const auto setter = [field](Class& object, const Value& value) {
            object.*field = value;
        };

        return property(std::move(name), getter, setter, hint);
    }

    // Метод класса или его базы. Типы параметров и результата берутся из
    // сигнатуры, имена параметров (и значения по умолчанию) - из parameters,
    // по одному на каждый параметр:
    //   .method("lookAt", &Transform3D::lookAt, {{"target"}, {"up", kWorldUp}})
    // Список параметров можно дать и span: так его хранит регистрация, которая
    // выполняется позже (скрипт модуля игры).
    template <typename Result, typename Owner, typename... Args>
        requires std::derived_from<Class, Owner>
    ClassTypeBuilder& method(
        std::string name,
        Result (Owner::*function)(Args...),
        std::initializer_list<ClassMethodParameter> parameters = {}
    )
        requires(std::same_as<Root, Node> || std::same_as<Root, Component>)
    {
        return addMethod<Result, Args...>(std::move(name), function, std::span(parameters.begin(), parameters.size()));
    }

    template <typename Result, typename Owner, typename... Args>
        requires std::derived_from<Class, Owner>
    ClassTypeBuilder& method(
        std::string name,
        Result (Owner::*function)(Args...) const,
        std::initializer_list<ClassMethodParameter> parameters = {}
    )
        requires(std::same_as<Root, Node> || std::same_as<Root, Component>)
    {
        return addMethod<Result, Args...>(std::move(name), function, std::span(parameters.begin(), parameters.size()));
    }

    template <typename Result, typename Owner, typename... Args>
        requires std::derived_from<Class, Owner>
    ClassTypeBuilder& method(
        std::string name,
        Result (Owner::*function)(Args...),
        std::span<const ClassMethodParameter> parameters
    )
        requires(std::same_as<Root, Node> || std::same_as<Root, Component>)
    {
        return addMethod<Result, Args...>(std::move(name), function, parameters);
    }

    template <typename Result, typename Owner, typename... Args>
        requires std::derived_from<Class, Owner>
    ClassTypeBuilder& method(
        std::string name,
        Result (Owner::*function)(Args...) const,
        std::span<const ClassMethodParameter> parameters
    )
        requires(std::same_as<Root, Node> || std::same_as<Root, Component>)
    {
        return addMethod<Result, Args...>(std::move(name), function, parameters);
    }

    // Событие компонента с именами его параметров, по одному на каждый:
    //   .event("onScored", &Score::onScored, {"points"})
    template <typename Owner, typename... Args>
        requires std::derived_from<Class, Owner>
    ClassTypeBuilder& event(
        std::string name,
        Event<Args...> Owner::* member,
        std::initializer_list<std::string> parameterNames = {}
    )
        requires std::same_as<Root, Component>
    {
        return event(std::move(name), member, std::span(parameterNames.begin(), parameterNames.size()));
    }

    template <typename Owner, typename... Args>
        requires std::derived_from<Class, Owner>
    ClassTypeBuilder& event(
        std::string name,
        Event<Args...> Owner::* member,
        std::span<const std::string> parameterNames
    )
        requires std::same_as<Root, Component>
    {
        assert(parameterNames.size() == sizeof...(Args) && "event: name every parameter");
        const std::array<PropertyType, sizeof...(Args)> types{propertyTypeOf<std::remove_cvref_t<Args>>()...};
        EventInfo info{.name = std::move(name), .parameters = {}, .of = [member](Component& component) -> EventBase& {
                           return static_cast<Class&>(component).*member;
                       }};
        for (std::size_t i = 0; const std::string& parameter : parameterNames) {
            info.parameters.push_back({parameter, types[i++]});
        }
        std::string propertyName = info.name;
        Base::addEvent(std::move(info));
        // Постоянные подписчики - свойство с тем же именем: так они пишутся в
        // файл, правятся в инспекторе с отменой и копируются в Play.
        const auto targets = [member](const Class& object) -> const std::vector<EventTarget>& {
            return (object.*member).targets();
        };
        const auto setTargets = [member](Class& object, const std::vector<EventTarget>& value) {
            (object.*member).setTargets(object, value);
        };

        return property(std::move(propertyName), targets, setTargets, PropertyHint::EventTargets);
    }

    //   registry.registerComponent<MeshRenderer>().requiresNode<Node3D>();
    template <std::derived_from<Node> NodeType>
    ClassTypeBuilder& requiresNode()
        requires std::same_as<Root, Component>
    {
        return requiresNode(std::string(typeNameOf<NodeType>()));
    }

    template <std::derived_from<Component> Required>
    ClassTypeBuilder& requiresComponent()
        requires std::same_as<Root, Component>
    {
        return requiresComponent(std::string(typeNameOf<Required>()));
    }

private:
    template <typename Result, typename... Args, typename Function>
    ClassTypeBuilder& addMethod(std::string name, Function function, std::span<const ClassMethodParameter> names)
    {
        assert(names.size() == sizeof...(Args) && "method: name every parameter");
        const std::array<PropertyType, sizeof...(Args)> types{propertyTypeOf<std::remove_cvref_t<Args>>()...};
        std::vector<MethodParameter> parameters;
        parameters.reserve(names.size());
        for (std::size_t i = 0; const ClassMethodParameter& parameter : names) {
            parameters.push_back({parameter.name, types[i++], parameter.defaultValue});
        }
        std::optional<PropertyType> result;
        if constexpr (!std::is_void_v<Result>) {
            result = propertyTypeOf<std::remove_cvref_t<Result>>();
        }

        const auto invoke = [function](Root& object, std::span<const PropertyValue> arguments) {
            return call<Result, Args...>(
                function,
                static_cast<Class&>(object),
                arguments,
                std::index_sequence_for<Args...>{}
            );
        };
        if constexpr (std::same_as<Root, Node>) {
            return method(Method::forNode(std::move(name), std::move(parameters), result, invoke));
        } else {
            return method(Method::forComponent(std::move(name), std::move(parameters), result, invoke));
        }
    }

    template <typename Result, typename... Args, typename Function, std::size_t... Indices>
    static std::optional<PropertyValue> call(
        Function function,
        Class& object,
        std::span<const PropertyValue> arguments,
        std::index_sequence<Indices...>
    )
    {
        if constexpr (std::is_void_v<Result>) {
            std::invoke(function, object, argument<Args>(arguments[Indices])...);

            return std::nullopt;
        } else {
            using Storage = PropertyStorage<std::remove_cvref_t<Result>>;

            return PropertyValue(
                std::in_place_type<typename Storage::Type>,
                Storage::store(std::invoke(function, object, argument<Args>(arguments[Indices])...))
            );
        }
    }

    template <typename Arg>
    static decltype(auto) argument(const PropertyValue& value)
    {
        using Storage = PropertyStorage<std::remove_cvref_t<Arg>>;

        return Storage::load(std::get<typename Storage::Type>(value));
    }
};

} // namespace yqlora
