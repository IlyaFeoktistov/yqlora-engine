#pragma once

// Только для кода движка, который включает Jolt (Jolt.h - раньше этого файла).

#include "yqlora/core/WarnOnce.hpp"
#include "yqlora/math/Math.hpp"
#include "yqlora/scene/NodeId.hpp"
#include "yqlora/scene/ObjectId.hpp"

#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Constraints/TwoBodyConstraint.h>

#include <map>
#include <optional>

namespace JPH {

class PhysicsSystem;

} // namespace JPH

namespace yqlora {

class CollisionIgnores;
class Joint;
class PhysicsBodies;
class Scene;

// Точка и две оси скрепления в осях узла (или мира).
struct JointFrame {
    Vec3 point{0.0f};
    Vec3 axis{0.0f, 1.0f, 0.0f};
    Vec3 normal{1.0f, 0.0f, 0.0f};
};

// Скрепления мира сцены (Joint) ограничениями Jolt. Создаются только в
// идущей сцене, после перестройки тел (sync перед шагом). Точка и оси на
// другом теле берутся по сцене при первом создании и запоминаются в осях
// его узла, поэтому перестройка тела скрепление не сдвигает. Тело, которое
// уходит из мира, сначала снимает свои скрепления (bodyRemoved). После шага
// - нагрузка и разрыв.
class PhysicsJoints {
public:
    PhysicsJoints(Scene& scene, JPH::PhysicsSystem& system, PhysicsBodies& bodies, CollisionIgnores& ignores);
    ~PhysicsJoints();

    PhysicsJoints(const PhysicsJoints&) = delete;
    PhysicsJoints& operator=(const PhysicsJoints&) = delete;

    void changed(Joint& joint);
    void tuned(Joint& joint);
    void left(const Joint& joint);

    void sync();
    // Сила и момент шага, разрыв выше пределов; stepSeconds - длина шага.
    void afterStep(float stepSeconds);

    // Нагрузка последнего шага; без скрепления - нули.
    float force(const Joint& joint) const;
    float torque(const Joint& joint) const;
    // Угол петли, радианы, и сдвиг слайдера, м; без скрепления - 0.
    float hingeAngle(const Joint& joint) const;
    float sliderPosition(const Joint& joint) const;

private:
    struct Entry {
        Joint* joint = nullptr;
        JPH::Ref<JPH::TwoBodyConstraint> constraint;
        JPH::BodyID own;
        JPH::BodyID connected;
        NodeId ownOwner = 0;
        NodeId connectedOwner = 0;
        // Рамка на другом теле в осях его узла; у мира - в осях мира.
        std::optional<JointFrame> connectedFrame;
        float force = 0.0f;
        float torque = 0.0f;
        // Тела пары не сталкиваются (CollisionIgnores::addBodyPair).
        bool ignoring = false;
        bool dirty = true;
        bool tune = false;
    };

    void bodyRemoved(JPH::BodyID id);
    void build(Entry& entry, JPH::BodyID own, JPH::BodyID connected, NodeId connectedOwner);
    void remove(Entry& entry);
    void configure(Entry& entry);
    const Entry* find(const Joint& joint) const;

    Scene& m_scene;
    JPH::PhysicsSystem& m_system;
    PhysicsBodies& m_bodies;
    CollisionIgnores& m_ignores;
    // По ID компонентов: разрывы и создание идут в устойчивом порядке.
    std::map<ComponentId, Entry> m_joints;
    WarnOnce m_warnings;
};

} // namespace yqlora
