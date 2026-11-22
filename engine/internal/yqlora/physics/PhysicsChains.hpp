#pragma once

// Только для кода движка, который включает Jolt (Jolt.h - раньше этого файла).

#include "yqlora/core/WarnOnce.hpp"
#include "yqlora/physics/BodyPose.hpp"
#include "yqlora/physics/JoltContacts.hpp"
#include "yqlora/scene/NodeId.hpp"
#include "yqlora/scene/ObjectId.hpp"

#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Constraints/SwingTwistConstraint.h>

#include <map>
#include <vector>

namespace JPH {

class PhysicsSystem;

} // namespace JPH

namespace yqlora {

class PhysicsBodies;
class PhysicsChain;
class PhysicsMaterialCache;
class ResourceCache;
class Scene;

// Цепи тел мира сцены (PhysicsChain): у каждой свои динамические тела
// звеньев и ограничения качания и кручения между соседями. Звенья видят
// другие тела, запросы и касания как тело узла цепи с номером звена
// (PhysicsBodies::addChainLink). Узел цепи и узлы звеньев пишутся после
// шага со сглаживанием; узел цепи, сдвинутый кодом или редактором,
// переносит цепь целиком, не меняя её изгиба. Устройство -
// docs/architecture/physics.md, раздел "Цепи тел".
class PhysicsChains {
public:
    PhysicsChains(Scene& scene, JPH::PhysicsSystem& system, PhysicsBodies& bodies, PhysicsMaterialCache& materials);
    ~PhysicsChains();

    PhysicsChains(const PhysicsChains&) = delete;
    PhysicsChains& operator=(const PhysicsChains&) = delete;

    // Цепь вошла в сцену или её звенья нужно построить заново.
    void changed(PhysicsChain& chain);
    // Мышцы цепи изменились.
    void tuned(PhysicsChain& chain);
    void left(const PhysicsChain& chain);

    // Строит и перестраивает звенья (участие в игре, слой, свойства),
    // переносит цепи сдвинутых узлов и применяет мышцы.
    void sync();
    // Перед шагом: сцепление звеньев, касающихся земли.
    void beforeStep();
    // После шага: положения звеньев и их касания земли из касаний шага.
    void afterStep(const std::vector<RawContact>& contacts);
    // Узлы цепей - положение между двумя последними шагами (fraction - доля шага).
    void interpolate(float fraction);
    // Будит звенья: сменились значения материалов.
    void wakeAll();

    // Изгиб шарнира index сейчас, градусы (вбок, вверх); центр и скорость
    // звена index в мире. Без звеньев - нули.
    Vec2 bend(const PhysicsChain& chain, int index) const;
    Vec3 linkPosition(const PhysicsChain& chain, int index) const;
    Vec3 linkVelocity(const PhysicsChain& chain, int index) const;

private:
    struct Link {
        JPH::BodyID id;
        // Поза в осях узла цепи при создании: туда звено встаёт заново.
        BodyPose rest;
        BodyPose previous;
        BodyPose current;
        // Нормаль земли под звеном (наружу из земли) и тело земли после
        // последнего шага; нулевая - звено земли не касается.
        Vec3 groundNormal{0.0f};
        JPH::BodyID ground;
    };

    // Узел, который следует за звеном: поза узла в осях звена при создании.
    struct Follower {
        NodeId node = 0;
        BodyPose offset;
    };

    struct Entry {
        PhysicsChain* chain = nullptr;
        std::vector<Link> links;
        std::vector<JPH::Ref<JPH::SwingTwistConstraint>> joints;
        // Поза узла цепи в осях первого звена.
        BodyPose headOffset;
        // Узлы звеньев по номерам; пусто - звено без узла.
        std::vector<Follower> followers;
        // Положение, которое цепь последней записала в узел: отличие - узел
        // сдвинули кодом или редактором.
        BodyPose remembered;
        // Слой узла, с которым построены звенья.
        int layer = 0;
        bool dirty = true;
        bool tune = false;
    };

    void build(Entry& entry);
    void destroy(Entry& entry);
    void configure(Entry& entry);
    // Переносит звенья так, что узел цепи встаёт в pose, а изгиб остаётся.
    void moveTo(Entry& entry, const BodyPose& pose);
    BodyPose linkPose(const Link& link) const;
    void write(Entry& entry, const std::vector<BodyPose>& poses);
    const Link* linkOf(const PhysicsChain& chain, int index) const;

    Scene& m_scene;
    JPH::PhysicsSystem& m_system;
    PhysicsBodies& m_bodies;
    PhysicsMaterialCache& m_materials;
    ResourceCache& m_resources;
    // По ID компонентов: звенья строятся в устойчивом порядке.
    std::map<ComponentId, Entry> m_chains;
    // Группа столкновений следующей цепи: звенья одной цепи в одной группе.
    std::uint32_t m_nextGroup = 0;
    WarnOnce m_warnings;
};

} // namespace yqlora
