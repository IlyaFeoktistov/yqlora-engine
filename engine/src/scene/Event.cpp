#include "yqlora/scene/Event.hpp"

#include "yqlora/core/Log.hpp"
#include "yqlora/reflection/ObjectAccess.hpp"
#include "yqlora/reflection/TypeRegistry.hpp"
#include "yqlora/scene/Component.hpp"
#include "yqlora/scene/Node.hpp"
#include "yqlora/scene/Scene.hpp"

#include <algorithm>
#include <exception>
#include <optional>
#include <string_view>

namespace yqlora {

namespace detail {

// Подписчики события. Общий для события и его подписок: подписка, пережившая
// событие, видит, что его уже нет.
struct EventSlots {
    struct Slot {
        std::uint64_t id = 0;
        // Метка жизни владельца: гаснет, когда он уходит из сцены.
        std::weak_ptr<const void> owner;
        // Для сообщения в лог; читается, только пока метка жива.
        const Component* ownerComponent = nullptr;
        std::function<void(const void*)> typed;
        EventBase::ValueListener values;
        bool connected = true;
    };

    void disconnect(std::uint64_t id)
    {
        const auto it = std::ranges::find(slots, id, &Slot::id);
        if (it == slots.end()) {
            return;
        }
        // Во время вызова список не трогается: снятая подписка только помечается.
        if (emitting > 0) {
            it->connected = false;
        } else {
            slots.erase(it);
        }
    }

    bool connected(std::uint64_t id) const
    {
        const auto it = std::ranges::find(slots, id, &Slot::id);

        return it != slots.end() && it->connected && !it->owner.expired();
    }

    void compact()
    {
        std::erase_if(slots, [](const Slot& slot) {
            return !slot.connected || slot.owner.expired();
        });
    }

    std::vector<Slot> slots;
    std::uint64_t nextId = 1;
    int emitting = 0;
};

} // namespace detail

Connection::Connection(std::weak_ptr<detail::EventSlots> slots, std::uint64_t id)
    : m_slots(std::move(slots))
    , m_id(id)
{
}

Connection::~Connection()
{
    disconnect();
}

Connection::Connection(Connection&& other) noexcept
    : m_slots(std::move(other.m_slots))
    , m_id(std::exchange(other.m_id, 0))
{
}

Connection& Connection::operator=(Connection&& other) noexcept
{
    if (this != &other) {
        disconnect();
        m_slots = std::move(other.m_slots);
        m_id = std::exchange(other.m_id, 0);
    }

    return *this;
}

void Connection::disconnect()
{
    if (const auto slots = m_slots.lock(); slots && m_id != 0) {
        slots->disconnect(m_id);
    }
    release();
}

void Connection::release()
{
    m_slots.reset();
    m_id = 0;
}

bool Connection::connected() const
{
    const auto slots = m_slots.lock();

    return slots && slots->connected(m_id);
}

EventBase::EventBase()
    : m_slots(std::make_shared<detail::EventSlots>())
{
}

EventBase::~EventBase() = default;

Connection EventBase::connectValues(Component& owner, ValueListener listener)
{
    const std::uint64_t id = m_slots->nextId++;
    m_slots->slots.push_back({
        .id = id,
        .owner = owner.lifetime(),
        .ownerComponent = &owner,
        .typed = {},
        .values = std::move(listener),
    });

    return Connection(m_slots, id);
}

Connection EventBase::connectTyped(Component& owner, TypedCall call)
{
    const std::uint64_t id = m_slots->nextId++;
    m_slots->slots.push_back({
        .id = id,
        .owner = owner.lifetime(),
        .ownerComponent = &owner,
        .typed = std::move(call),
        .values = {},
    });

    return Connection(m_slots, id);
}

std::size_t EventBase::listenerCount() const
{
    return static_cast<std::size_t>(std::ranges::count_if(m_slots->slots, [](const detail::EventSlots::Slot& slot) {
        return slot.connected && !slot.owner.expired();
    }));
}

void EventBase::setTargets(Component& owner, std::vector<EventTarget> targets)
{
    m_owner = &owner;
    m_targets = std::move(targets);
    m_reported.assign(m_targets.size(), false);
}

void EventBase::callTargets(std::span<const PropertyValue> arguments)
{
    const Scene* scene = m_owner && m_owner->owner() ? m_owner->owner()->scene() : nullptr;
    if (!scene || !scene->playing()) {
        return;
    }
    const TypeRegistry& registry = scene->services().registry();
    for (std::size_t i = 0; i < m_targets.size(); ++i) {
        const EventTarget& target = m_targets[i];
        MethodCallResult result;
        try {
            const auto call = [&](auto& object) {
                result = callMethod(object, target.method, arguments, registry);
                // Метод без параметров слушает любое событие.
                if (!result.called && !arguments.empty()) {
                    result = callMethod(object, target.method, {}, registry);
                }
            };
            if (target.target.component != 0) {
                if (Component* component = scene->find(target.target)) {
                    call(*component);
                }
            } else if (Node* node = scene->find(NodeRef{target.target.node})) {
                call(*node);
            }
        } catch (const std::exception& error) {
            log::error("Event target '{}' threw: {}", target.method, error.what());
            continue;
        }
        if (!result.called && !m_reported[i]) {
            m_reported[i] = true;
            log::warn(
                "Event target '{}' of component '{}' is not called: no such node, component or method with fitting "
                "parameters",
                target.method,
                m_owner->typeName()
            );
        }
    }
}

void EventBase::emitArguments(const void* arguments, const std::function<std::vector<PropertyValue>()>& values)
{
    // Список держится живым на время вызова: подписчик может уничтожить само событие.
    const std::shared_ptr<detail::EventSlots> slots = m_slots;
    ++slots->emitting;
    std::optional<std::vector<PropertyValue>> converted;
    // Добавленные во время вызова лежат за этой границей и ждут следующего.
    const std::size_t count = slots->slots.size();
    for (std::size_t i = 0; i < count; ++i) {
        // По номеру, а не ссылке: новые подписки могут переложить список.
        if (!slots->slots[i].connected) {
            continue;
        }
        const auto alive = slots->slots[i].owner.lock();
        if (!alive) {
            continue;
        }
        // Тип - до вызова: обработчик может уничтожить своего владельца.
        const std::string_view ownerType = slots->slots[i].ownerComponent->typeName();
        try {
            if (slots->slots[i].typed) {
                const auto call = slots->slots[i].typed;
                call(arguments);
            } else {
                if (!converted) {
                    converted = values();
                }
                const auto call = slots->slots[i].values;
                call(*converted);
            }
        } catch (const std::exception& error) {
            log::error("Event listener of component '{}' threw: {}", ownerType, error.what());
        } catch (...) {
            log::error("Event listener of component '{}' threw an unknown exception", ownerType);
        }
    }
    if (--slots->emitting == 0) {
        slots->compact();
    }
    if (!m_targets.empty()) {
        if (!converted) {
            converted = values();
        }
        callTargets(*converted);
    }
}

} // namespace yqlora
