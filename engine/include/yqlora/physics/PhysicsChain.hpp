#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/math/Math.hpp"
#include "yqlora/physics/Collision.hpp"
#include "yqlora/physics/PhysicsMaterialData.hpp"
#include "yqlora/reflection/Property.hpp"
#include "yqlora/scene/Component.hpp"
#include "yqlora/scene/Event.hpp"

#include <vector>

namespace yqlora {

// Пределы свойств цепи.
inline constexpr int kMinChainSegments = 2;
inline constexpr int kMaxChainSegments = 256;
inline constexpr float kMinChainSize = 0.01f;
inline constexpr float kMaxChainBendDegrees = 180.0f;

// Длинное тело, которое гнётся по длине: змея, червь, хвост, щупальце.
// Звенья - капсулы радиуса radius, уложенные от узла назад, вдоль его
// локальной оси +Z, на длину length; соседей скрепляет качание и кручение в
// пределах с мотором (мышцей) к целевому изгибу. Узел цепи следует за
// первым звеном (головой), первые segments дочерних узлов Node3D - каждый за
// своим звеном, с той же позой относительно звена, что при создании цепи.
// Узел без RigidBody: тело и цепь двигали бы его оба. Устройство -
// docs/architecture/physics.md, раздел "Цепи тел".
class YQLORA_API PhysicsChain final : public Component {
public:
    YQLORA_TYPE(PhysicsChain, Component)

    PhysicsChain();

    // Число звеньев, от kMinChainSegments до kMaxChainSegments.
    int segments() const
    {
        return m_segments;
    }

    void setSegments(int segments);

    // Длина всей цепи и радиус звеньев, м. Радиус больше половины звена
    // делает звенья шарами.
    float length() const
    {
        return m_length;
    }

    void setLength(float length);

    float radius() const
    {
        return m_radius;
    }

    void setRadius(float radius);

    // Масса всей цепи, кг, поровну на звенья.
    float mass() const
    {
        return m_mass;
    }

    void setMass(float mass);

    // Насколько звено может отклониться от соседа (полуугол конуса) и
    // закрутиться вокруг своей оси в обе стороны, градусы.
    float bendLimit() const
    {
        return m_bendLimit;
    }

    void setBendLimit(float degrees);

    float twistLimit() const
    {
        return m_twistLimit;
    }

    void setTwistLimit(float degrees);

    // Мышцы - пружины к целевому изгибу: жёсткость, Н*м на радиан
    // отклонения, затухание, Н*м*с на радиан, и предельный момент мышцы,
    // Н*м. Жёсткость или момент 0 - мышц нет. Жёсткость в моменте, а не в
    // частоте, поэтому мышца держит груз (свисающий хвост), каким бы лёгким
    // ни было само звено.
    float stiffness() const
    {
        return m_stiffness;
    }

    void setStiffness(float stiffness);

    float damping() const
    {
        return m_damping;
    }

    void setDamping(float damping);

    float maxTorque() const
    {
        return m_maxTorque;
    }

    void setMaxTorque(float torque);

    // Сцепление с землёй: какую долю скорости звено, касающееся земли,
    // теряет за шаг физики поперёк своей оси (side) и вдоль неё (forward),
    // от 0 до 1. Змея ползёт, потому что вбок скользить труднее, чем вперёд.
    float sideGrip() const
    {
        return m_sideGrip;
    }

    void setSideGrip(float grip);

    float forwardGrip() const
    {
        return m_forwardGrip;
    }

    void setForwardGrip(float grip);

    // Трение и упругость звеньев; пусто - материал по умолчанию из раздела
    // Physics настроек проекта.
    Ref<PhysicsMaterialData> material() const
    {
        return m_material;
    }

    void setMaterial(Ref<PhysicsMaterialData> material);

    // Целевой изгиб шарнира index (между звеньями index и index + 1, от 0
    // до segments - 2), градусы: x - вбок (вокруг оси Y звена, к +X), y -
    // вверх (вокруг оси X звена, к +Y). Включает мышцы после relax().
    // Шарнир вне цепи - ничего.
    void setBend(int index, float side, float up);
    // Изгибы всех шарниров с начала; лишние не берутся, недостающие не меняются.
    void setBends(const std::vector<Vec2>& bends);
    Vec2 targetBend(int index) const;

    // Изгиб шарнира сейчас, в тех же осях; без звеньев или вне цепи - 0.
    Vec2 bend(int index) const;

    // Центр звена в мире, м, и его скорость, м/с; без звеньев или вне
    // цепи - 0.
    Vec3 linkPosition(int index) const;
    Vec3 linkVelocity(int index) const;

    // Выключает мышцы: цепь обмякает (мёртвая змея).
    void relax();

    bool relaxed() const
    {
        return m_relaxed;
    }

    // Звено коснулось коллайдера другого тела, касается после шага,
    // перестало: как у RigidBody, номер звена - Collision::link.
    Event<Collision> onCollisionEnter;
    Event<Collision> onCollisionStay;
    Event<Collision> onCollisionExit;

protected:
    void onEnterScene() override;
    void onExitScene() override;

private:
    // Звенья нужно построить заново (число, размеры, масса, пределы).
    void changed();
    // Мышцы и сцепление: применяются к готовым звеньям.
    void tuned();

    int m_segments = 8;
    float m_length = 2.0f;
    float m_radius = 0.1f;
    float m_mass = 4.0f;
    float m_bendLimit = 45.0f;
    float m_twistLimit = 10.0f;
    float m_stiffness = 50.0f;
    float m_damping = 2.0f;
    float m_maxTorque = 20.0f;
    float m_sideGrip = 0.0f;
    float m_forwardGrip = 0.0f;
    Ref<PhysicsMaterialData> m_material;
    bool m_relaxed = false;
    // Целевые изгибы шарниров, segments - 1.
    std::vector<Vec2> m_bends;
};

} // namespace yqlora
