#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/reflection/Property.hpp"
#include "yqlora/reflection/Record.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <tuple>
#include <utility>
#include <vector>

namespace yqlora {

class Component;

namespace detail {

struct EventSlots;

} // namespace detail

// Постоянный подписчик события, заданный в инспекторе и сохранённый со
// сценой: метод узла или его компонента по имени (component 0 - сам узел).
struct EventTarget {
    ComponentRef<> target;
    std::string method;

    bool operator==(const EventTarget&) const = default;
};

// Подписка на событие. Отписывается, когда её уничтожают или зовут
// disconnect(); release() оставляет подписку жить, пока жив её владелец.
class YQLORA_API Connection {
public:
    Connection() = default;
    Connection(std::weak_ptr<detail::EventSlots> slots, std::uint64_t id);
    ~Connection();

    Connection(Connection&& other) noexcept;
    Connection& operator=(Connection&& other) noexcept;
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    void disconnect();

    // Подписка больше не зависит от этого объекта: снимется вместе с
    // владельцем или событием.
    void release();

    bool connected() const;

private:
    std::weak_ptr<detail::EventSlots> m_slots;
    std::uint64_t m_id = 0;
};

// Общая часть всех событий: подписчики, их владельцы и правила вызова.
// Аргументы подписчик получает типизированно (Event<Args...>::connect) или
// значениями свойств (connectValues) - так слушают Luau, визуальные скрипты и
// постоянные подписчики сцены.
class YQLORA_API EventBase {
public:
    using ValueListener = std::function<void(std::span<const PropertyValue>)>;

    EventBase();
    ~EventBase();

    EventBase(const EventBase&) = delete;
    EventBase& operator=(const EventBase&) = delete;

    Connection connectValues(Component& owner, ValueListener listener);

    // Живые подписки: их владельцы ещё в сцене.
    std::size_t listenerCount() const;

    // Постоянные подписчики. Вызываются после подписчиков из кода, по порядку
    // списка, и только в идущей сцене (Scene::playing). Метод с параметрами
    // получает аргументы события, без параметров - вызывается без них; цель,
    // которой нет или чей метод не подходит, пропускается с сообщением в лог
    // один раз. owner - компонент, которому принадлежит событие: через его
    // сцену находятся цели.
    const std::vector<EventTarget>& targets() const
    {
        return m_targets;
    }

    void setTargets(Component& owner, std::vector<EventTarget> targets);

protected:
    // Типизированный вызов получает адрес кортежа аргументов.
    using TypedCall = std::function<void(const void* arguments)>;

    Connection connectTyped(Component& owner, TypedCall call);

    // Вызывает подписчиков по порядку подписки. Добавленный во время вызова
    // впервые вызывается следующим, удалённый больше не вызывается;
    // исключение подписчика пишется в лог, остальные вызываются.
    void emitArguments(const void* arguments, const std::function<std::vector<PropertyValue>()>& values);

private:
    void callTargets(std::span<const PropertyValue> arguments);

    std::shared_ptr<detail::EventSlots> m_slots;
    Component* m_owner = nullptr;
    std::vector<EventTarget> m_targets;
    // Цели, о которых лог уже сказал, что они не вызываются.
    std::vector<bool> m_reported;
};

// Событие компонента: член класса, регистрируется в реестре как .event(...).
//   Event<int> onScored;
//   onScored.connect(*this, [](int points) { ... });
//   onScored.emit(10);
template <typename... Args>
class Event final : public EventBase {
public:
    template <typename Listener>
        requires std::invocable<Listener&, const Args&...>
    Connection connect(Component& owner, Listener listener)
    {
        return connectTyped(owner, [listener = std::move(listener)](const void* arguments) mutable {
            std::apply(listener, *static_cast<const std::tuple<Args...>*>(arguments));
        });
    }

    void emit(const Args&... arguments)
    {
        const std::tuple<Args...> packed(arguments...);
        emitArguments(&packed, [&packed] {
            return std::apply(
                [](const Args&... values) {
                    return std::vector<PropertyValue>{PropertyValue(
                        std::in_place_type<typename PropertyStorage<Args>::Type>,
                        PropertyStorage<Args>::store(values)
                    )...};
                },
                packed
            );
        });
    }
};

} // namespace yqlora

YQLORA_RECORD(
    yqlora::EventTarget,
    field("target", &yqlora::EventTarget::target),
    field("method", &yqlora::EventTarget::method)
)
