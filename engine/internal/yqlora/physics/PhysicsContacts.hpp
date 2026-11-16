#pragma once

// Только для кода движка, который включает Jolt (Jolt.h - раньше этого файла).

#include "yqlora/physics/JoltContacts.hpp"
#include "yqlora/physics/PhysicsBodies.hpp"

#include <compare>
#include <map>
#include <vector>

namespace yqlora {

class Scene;
struct Collision;

// События касаний после шага: касания шага (JoltContacts) сравниваются с
// прошлыми. Новое - вход, бывшее - продолжение, пропавшее - выход; пара,
// оба тела которой не движутся (спят или неподвижны), продолжается без
// событий, потому что Jolt о спящих молчит. Столкновения - на пару
// коллайдеров, их получают RigidBody обеих сторон; триггеры - на пару
// триггера и тела, их получает коллайдер-триггер. Порядок: выходы, входы,
// продолжения, каждый по ID узлов и коллайдеров, поэтому игра ведёт себя
// одинаково при каждом запуске. Тело, ушедшее из сцены, выходит из своих
// касаний после следующего шага: событие получает сторона, которая ещё в
// сцене.
class PhysicsContacts {
public:
    PhysicsContacts(Scene& scene, const PhysicsBodies& bodies);

    void deliver(const std::vector<RawContact>& contacts);

    struct CollisionKey {
        NodeId firstOwner = 0;
        ComponentId firstCollider = 0;
        NodeId secondOwner = 0;
        ComponentId secondCollider = 0;

        auto operator<=>(const CollisionKey&) const = default;
    };

    // Стороны по порядку ключа; нормаль - куда второе толкает из первого,
    // скорость - первого относительно второго.
    struct Touch {
        ContactSide first;
        ContactSide second;
        std::vector<Vec3> points;
        Vec3 normal{0.0f};
        Vec3 relativeVelocity{0.0f};
        float impulse = 0.0f;
    };

    // Касания пар коллайдеров после последнего шага.
    const std::map<CollisionKey, Touch>& collisions() const
    {
        return m_collisions;
    }

private:
    struct TriggerKey {
        NodeId triggerOwner = 0;
        ComponentId trigger = 0;
        NodeId other = 0;

        auto operator<=>(const TriggerKey&) const = default;
    };

    struct TriggerTouch {
        BodyPart trigger;
        NodeId other = 0;
    };

    enum class Phase {
        Enter,
        Stay,
        Exit,
    };

    void collect(const std::vector<RawContact>& contacts);
    void emitCollision(Phase phase, const Touch& touch);
    void emitTrigger(bool enter, const TriggerTouch& touch);

    Scene& m_scene;
    const PhysicsBodies& m_bodies;
    std::map<CollisionKey, Touch> m_collisions;
    std::map<TriggerKey, TriggerTouch> m_triggers;
    // Касания шага, собранные collect.
    std::map<CollisionKey, Touch> m_stepCollisions;
    std::map<TriggerKey, TriggerTouch> m_stepTriggers;
};

} // namespace yqlora
