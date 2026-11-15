// Jolt.h - первым: остальные заголовки Jolt на него опираются.
#include <Jolt/Jolt.h>

#include "yqlora/physics/PhysicsContacts.hpp"

#include "yqlora/physics/Collider.hpp"
#include "yqlora/physics/Collision.hpp"
#include "yqlora/physics/RigidBody.hpp"
#include "yqlora/scene/Scene.hpp"

#include <set>
#include <tuple>
#include <utility>

namespace yqlora {

namespace {

bool before(const ContactSide& a, const ContactSide& b)
{
    return std::tie(a.owner, a.part.collider) < std::tie(b.owner, b.part.collider);
}

ComponentRef<Collider> colliderRef(const BodyPart& part)
{
    return {part.node, part.collider};
}

} // namespace

PhysicsContacts::PhysicsContacts(Scene& scene, const PhysicsBodies& bodies)
    : m_scene(scene)
    , m_bodies(bodies)
{
}

void PhysicsContacts::collect(const std::vector<RawContact>& contacts)
{
    m_stepCollisions.clear();
    m_stepTriggers.clear();
    for (const RawContact& contact : contacts) {
        auto first = m_bodies.sideOf(contact.first, contact.firstPart);
        auto second = m_bodies.sideOf(contact.second, contact.secondPart);
        // Триггер не видит тело своего же узла, датчики друг друга - тоже.
        if (!first || !second || first->owner == second->owner || (first->trigger && second->trigger)) {
            continue;
        }
        if (first->trigger || second->trigger) {
            const ContactSide& trigger = first->trigger ? *first : *second;
            const NodeId other = first->trigger ? second->owner : first->owner;
            m_stepTriggers[{trigger.owner, trigger.part.collider, other}] = {trigger.part, other};
            continue;
        }
        Touch touch{
            .first = *first,
            .second = *second,
            .points = contact.points,
            .normal = contact.normal,
            .relativeVelocity = contact.relativeVelocity,
            .impulse = contact.impulse,
        };
        if (before(touch.second, touch.first)) {
            std::swap(touch.first, touch.second);
            touch.normal = -touch.normal;
            touch.relativeVelocity = -touch.relativeVelocity;
        }
        const CollisionKey key{
            touch.first.owner,
            touch.first.part.collider,
            touch.second.owner,
            touch.second.part.collider,
        };
        m_stepCollisions[key] = std::move(touch);
    }
}

void PhysicsContacts::deliver(const std::vector<RawContact>& contacts)
{
    collect(contacts);

    // Пропавшая пара, оба тела которой не движутся, - спит, а не разошлась.
    std::vector<Touch> exits;
    std::set<CollisionKey> asleep;
    for (auto& [key, touch] : m_collisions) {
        if (m_stepCollisions.contains(key)) {
            continue;
        }
        if (m_bodies.resting(key.firstCollider) && m_bodies.resting(key.secondCollider)) {
            asleep.insert(key);
            m_stepCollisions.emplace(key, std::move(touch));
            continue;
        }
        touch.points.clear();
        touch.impulse = 0.0f;
        exits.push_back(std::move(touch));
    }
    std::vector<TriggerTouch> triggerExits;
    for (const auto& [key, touch] : m_triggers) {
        if (!m_stepTriggers.contains(key)) {
            triggerExits.push_back(touch);
        }
    }
    std::vector<Touch> enters;
    std::vector<Touch> stays;
    for (const auto& [key, touch] : m_stepCollisions) {
        if (asleep.contains(key)) {
            continue;
        }
        if (m_collisions.contains(key)) {
            Touch stay = touch;
            stay.impulse = 0.0f;
            stays.push_back(std::move(stay));
        } else {
            enters.push_back(touch);
        }
    }
    std::vector<TriggerTouch> triggerEnters;
    for (const auto& [key, touch] : m_stepTriggers) {
        if (!m_triggers.contains(key)) {
            triggerEnters.push_back(touch);
        }
    }
    m_collisions = std::move(m_stepCollisions);
    m_triggers = std::move(m_stepTriggers);
    m_stepCollisions.clear();
    m_stepTriggers.clear();

    // Обработчики могут менять сцену: всё нужное уже скопировано.
    for (const Touch& touch : exits) {
        emitCollision(Phase::Exit, touch);
    }
    for (const TriggerTouch& touch : triggerExits) {
        emitTrigger(false, touch);
    }
    for (const Touch& touch : enters) {
        emitCollision(Phase::Enter, touch);
    }
    for (const TriggerTouch& touch : triggerEnters) {
        emitTrigger(true, touch);
    }
    for (const Touch& touch : stays) {
        emitCollision(Phase::Stay, touch);
    }
}

void PhysicsContacts::emitCollision(Phase phase, const Touch& touch)
{
    const auto emitTo = [&](const ContactSide& self, const ContactSide& other, float sign) {
        Node* node = m_scene.find(self.owner);
        RigidBody* body = node ? node->getComponent<RigidBody>() : nullptr;
        if (!body || !body->activeAndEnabled()) {
            return;
        }
        const Collision collision{
            .other = NodeRef{other.owner},
            .collider = colliderRef(self.part),
            .otherCollider = colliderRef(other.part),
            .points = touch.points,
            .normal = touch.normal * sign,
            .relativeVelocity = touch.relativeVelocity * -sign,
            .impulse = touch.impulse,
        };
        switch (phase) {
        case Phase::Enter: body->onCollisionEnter.emit(collision); break;
        case Phase::Stay: body->onCollisionStay.emit(collision); break;
        case Phase::Exit: body->onCollisionExit.emit(collision); break;
        }
    };
    // Нормаль смотрит из первого во второе: второе толкает первое против неё.
    emitTo(touch.first, touch.second, -1.0f);
    emitTo(touch.second, touch.first, 1.0f);
}

void PhysicsContacts::emitTrigger(bool enter, const TriggerTouch& touch)
{
    auto* trigger = m_scene.find(ComponentRef<Collider>{touch.trigger.node, touch.trigger.collider});
    if (!trigger || !trigger->activeAndEnabled()) {
        return;
    }
    (enter ? trigger->onTriggerEnter : trigger->onTriggerExit).emit(NodeRef{touch.other});
}

} // namespace yqlora
