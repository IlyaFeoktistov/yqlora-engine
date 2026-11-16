#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/math/Math.hpp"
#include "yqlora/physics/JointDefinition.hpp"
#include "yqlora/reflection/Property.hpp"
#include "yqlora/scene/Component.hpp"
#include "yqlora/scene/Event.hpp"

namespace yqlora {

// Больше шагов решателя на одно скрепление не поднять.
inline constexpr int kMaxJointSolverSteps = 255;

// Скрепление тела своего узла (RigidBody) с телом connectedBody или, если
// ссылка пустая, с миром: как они могут двигаться друг относительно друга.
// Вид скрепления - тип компонента. Точка крепления на другом теле
// вычисляется по сцене, когда скрепление создаётся в идущей сцене, так что
// связано то, что видно в редакторе. Устройство - docs/architecture/physics.md.
class YQLORA_API Joint : public Component {
public:
    YQLORA_TYPE(Joint, Component)

    // Узел другого тела (с RigidBody или с коллайдерами без него); пусто - мир.
    const NodeRef& connectedBody() const
    {
        return m_connectedBody;
    }

    void setConnectedBody(const NodeRef& body);

    // Точка крепления в осях своего узла.
    const Vec3& anchor() const
    {
        return m_anchor;
    }

    void setAnchor(const Vec3& anchor);

    // Сталкиваются ли скреплённые тела друг с другом.
    bool collideConnected() const
    {
        return m_collideConnected;
    }

    void setCollideConnected(bool collide);

    // Сила, Н, и момент, Н*м, выше которых скрепление рвётся; 0 - никогда.
    float breakForce() const
    {
        return m_breakForce;
    }

    void setBreakForce(float force);

    float breakTorque() const
    {
        return m_breakTorque;
    }

    void setBreakTorque(float torque);

    // Шагов решателя для этого скрепления (скорость и положение); 0 - из
    // раздела Physics настроек. Длинным цепям с тяжёлым грузом нужно больше.
    int solverStepsOverride() const
    {
        return m_solverStepsOverride;
    }

    void setSolverStepsOverride(int steps);

    // С какой силой, Н, и моментом, Н*м, скрепление держало тела в
    // последнем шаге; без ограничения - 0.
    float force() const;
    float torque() const;

    // Скрепление разорвалось: сила или момент шага выше предела. После
    // этого компонент выключен, а сцена не меняется.
    Event<> onBreak;

    virtual JointDefinition definition() const = 0;

protected:
    Joint() = default;

    // Скрепление нужно создать заново (точки, оси, тела).
    void changed();
    // Пределы, пружины и моторы: применяются к готовому скреплению.
    void tuned();

    void onEnterScene() override;
    void onExitScene() override;

private:
    NodeRef m_connectedBody;
    Vec3 m_anchor{0.0f};
    bool m_collideConnected = false;
    float m_breakForce = 0.0f;
    float m_breakTorque = 0.0f;
    int m_solverStepsOverride = 0;
};

// Скрепление с осью: петля и слайдер.
class YQLORA_API AxisJoint : public Joint {
public:
    YQLORA_TYPE(AxisJoint, Joint)

    // Ось в осях своего узла; нулевая не принимается.
    const Vec3& axis() const
    {
        return m_axis;
    }

    void setAxis(const Vec3& axis);

    const JointLimits& limits() const
    {
        return m_limits;
    }

    void setLimits(const JointLimits& limits);

    // Мягкость пределов.
    const JointSpring& limitSpring() const
    {
        return m_limitSpring;
    }

    void setLimitSpring(const JointSpring& spring);

    JointMotorMode motor() const
    {
        return m_motor.mode;
    }

    void setMotor(JointMotorMode mode);

    // Цель мотора: скорость или положение по режиму. Скрипт меняет её,
    // чтобы открыть дверь или крутить колесо.
    float motorTarget() const
    {
        return m_motor.target;
    }

    void setMotorTarget(float target);

    float maxMotorForce() const
    {
        return m_motor.maxForce;
    }

    void setMaxMotorForce(float force);

protected:
    explicit AxisJoint(const Vec3& axis)
        : m_axis(axis)
    {
    }

    const JointMotor& motorSettings() const
    {
        return m_motor;
    }

private:
    Vec3 m_axis;
    JointLimits m_limits;
    JointSpring m_limitSpring;
    JointMotor m_motor;
};

// Без движения друг относительно друга: склейка, разрушаемые стены.
class YQLORA_API FixedJoint final : public Joint {
public:
    YQLORA_TYPE(FixedJoint, Joint)

    JointDefinition definition() const override
    {
        return FixedJointDefinition{};
    }
};

// Одна общая точка, поворот свободный: цепи, верёвки, маятники.
class YQLORA_API PointJoint final : public Joint {
public:
    YQLORA_TYPE(PointJoint, Joint)

    JointDefinition definition() const override
    {
        return PointJointDefinition{};
    }
};

// Поворот вокруг оси: двери, колёса, рычаги. Пределы и мотор - в градусах.
class YQLORA_API HingeJoint final : public AxisJoint {
public:
    YQLORA_TYPE(HingeJoint, AxisJoint)

    HingeJoint()
        : AxisJoint({0.0f, 1.0f, 0.0f})
    {
    }

    // Угол от положения при создании, градусы; без скрепления - 0.
    float angle() const;

    JointDefinition definition() const override;
};

// Движение вдоль оси: ящики, поршни, лифты. Пределы и мотор - в метрах.
class YQLORA_API SliderJoint final : public AxisJoint {
public:
    YQLORA_TYPE(SliderJoint, AxisJoint)

    SliderJoint()
        : AxisJoint({1.0f, 0.0f, 0.0f})
    {
    }

    // Сдвиг от положения при создании, м; без скрепления - 0.
    float position() const;

    JointDefinition definition() const override;
};

// Расстояние между точками крепления в пределах, с пружиной: провисающие
// верёвки, резинки.
class YQLORA_API DistanceJoint final : public Joint {
public:
    YQLORA_TYPE(DistanceJoint, Joint)

    // Оба меньше нуля - расстояние при создании.
    float minDistance() const
    {
        return m_definition.minDistance;
    }

    void setMinDistance(float distance);

    float maxDistance() const
    {
        return m_definition.maxDistance;
    }

    void setMaxDistance(float distance);

    const JointSpring& spring() const
    {
        return m_definition.spring;
    }

    void setSpring(const JointSpring& spring);

    // Точка крепления на другом теле, в осях его узла (у мира - точка мира).
    const Vec3& connectedAnchor() const
    {
        return m_definition.connectedAnchor;
    }

    void setConnectedAnchor(const Vec3& anchor);

    JointDefinition definition() const override
    {
        return m_definition;
    }

private:
    DistanceJointDefinition m_definition;
};

} // namespace yqlora
