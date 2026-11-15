#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/math/Math.hpp"
#include "yqlora/physics/Collision.hpp"
#include "yqlora/reflection/EnumNames.hpp"
#include "yqlora/reflection/Record.hpp"
#include "yqlora/scene/Component.hpp"
#include "yqlora/scene/Event.hpp"

#include <cstdint>

namespace yqlora {

// Как тело движется: Static - никогда (стена, пол), Kinematic - только
// когда его двигает код или редактор, увозя то, что на нём стоит
// (платформа, дверь), Dynamic - по силам и столкновениям.
enum class BodyMotion : std::uint8_t {
    Static,
    Kinematic,
    Dynamic,
};

// Как addForce и addTorque понимают вектор: Force - сила, Н (момент, Н*м),
// действует весь следующий шаг; Acceleration - ускорение, м/с^2, той же
// длительности, без учёта массы; Impulse - импульс, Н*с, сразу;
// VelocityChange - прибавка скорости, м/с (рад/с), сразу, без учёта массы.
enum class ForceMode : std::uint8_t {
    Force,
    Acceleration,
    Impulse,
    VelocityChange,
};

// Оси мира, по которым тело не движется (или вокруг которых не вращается):
// игра в плоскости, дверь, которая только поворачивается.
struct AxisLocks {
    bool x = false;
    bool y = false;
    bool z = false;

    bool operator==(const AxisLocks&) const = default;
};

// Самая малая масса тела, кг: у тела с нулевой массой нет инерции.
inline constexpr float kMinBodyMass = 0.001f;
// Затухание скорости по умолчанию, как у Jolt: тело в воздухе почти не тормозит.
inline constexpr float kDefaultBodyDamping = 0.05f;

// Твёрдое тело на узле Node3D: его фигура - коллайдеры узла и потомков до
// ближайшего узла со своим RigidBody (тело без коллайдеров падает, но ни с
// чем не сталкивается). Положение тела - положение узла. Устройство -
// docs/architecture/physics.md.
class YQLORA_API RigidBody final : public Component {
public:
    YQLORA_TYPE(RigidBody, Component)

    BodyMotion motion() const
    {
        return m_motion;
    }

    void setMotion(BodyMotion motion);

    // Масса всего тела, кг, не меньше kMinBodyMass; инерция - по фигуре.
    float mass() const
    {
        return m_mass;
    }

    void setMass(float mass);

    // Доля скорости, которую тело теряет за секунду, не меньше 0.
    float linearDamping() const
    {
        return m_linearDamping;
    }

    float angularDamping() const
    {
        return m_angularDamping;
    }

    void setLinearDamping(float damping);
    void setAngularDamping(float damping);

    // Множитель гравитации мира: 0 - тело её не чувствует.
    float gravityScale() const
    {
        return m_gravityScale;
    }

    void setGravityScale(float scale);

    // Узел показывает положение между двумя последними шагами физики, и
    // движение плавное при любой частоте кадров. Само тело оно не трогает.
    bool interpolation() const
    {
        return m_interpolation;
    }

    void setInterpolation(bool interpolation);

    // Быстрое маленькое тело (пуля, мяч) проверяется по всему пути за шаг,
    // поэтому не проскакивает тонкую стену. Дороже обычного.
    bool continuousCollision() const
    {
        return m_continuousCollision;
    }

    void setContinuousCollision(bool continuous);

    // Запрещённые оси мира. Запрещено всё - тело неподвижно, и лог говорит
    // об этом один раз.
    const AxisLocks& lockPosition() const
    {
        return m_lockPosition;
    }

    const AxisLocks& lockRotation() const
    {
        return m_lockRotation;
    }

    void setLockPosition(const AxisLocks& locks);
    void setLockRotation(const AxisLocks& locks);

    // Тело начинает спящим: не падает и не считается, пока его не тронут
    // (груда ящиков, которая стоит до первого удара).
    bool startAsleep() const
    {
        return m_startAsleep;
    }

    void setStartAsleep(bool asleep);

    // Скорость тела в мире, м/с, и угловая, рад/с (вокруг осей мира).
    // Задаётся динамическому телу; кинематическое следует за узлом. Без
    // тела (компонент не в сцене или выключен) - ноль, задание - ничего.
    Vec3 velocity() const;
    void setVelocity(const Vec3& velocity);
    Vec3 angularVelocity() const;
    void setAngularVelocity(const Vec3& velocity);

    // Сила, импульс или прибавка скорости в центре масс и в точке мира
    // point (тогда и поворот), и момент вокруг центра масс; только
    // динамическому телу, будит его. Сила из onFixedUpdate действует в том
    // же шаге, из onUpdate - в следующем.
    void addForce(const Vec3& force, ForceMode mode = ForceMode::Force);
    void addForceAtPoint(const Vec3& force, const Vec3& point, ForceMode mode = ForceMode::Force);
    void addTorque(const Vec3& torque, ForceMode mode = ForceMode::Force);

    // Цель кинематического тела в мире: узел встаёт туда сразу, а тело
    // доходит за следующий шаг и везёт то, что на нём стоит.
    void moveTo(const Vec3& position);
    void rotateTo(const Quat& rotation);
    // Переносит узел и тело любого вида сразу, без пути (то, что на пути,
    // не задевается); скорость остаётся.
    void teleport(const Vec3& position, const Quat& rotation);

    // Сон: тело не считается, пока его не тронут. Неподвижное спит всегда.
    void sleep();
    void wakeUp();
    bool isSleeping() const;

    // Коллайдер тела (и коллайдеры потомков, из которых оно собрано) начал
    // касаться коллайдера другого тела, касается после шага, перестал.
    // Приходят после шага физики: сначала выходы, затем входы, затем
    // продолжения, каждый по ID узлов. Спящая пара продолжений не шлёт и не
    // выходит, пока не проснётся.
    Event<Collision> onCollisionEnter;
    Event<Collision> onCollisionStay;
    Event<Collision> onCollisionExit;

protected:
    void onEnterScene() override;
    void onExitScene() override;

private:
    // Тело изменилось: физика перестроит его перед следующим шагом.
    void changed();

    BodyMotion m_motion = BodyMotion::Dynamic;
    float m_mass = 1.0f;
    float m_linearDamping = kDefaultBodyDamping;
    float m_angularDamping = kDefaultBodyDamping;
    float m_gravityScale = 1.0f;
    bool m_interpolation = true;
    bool m_continuousCollision = false;
    AxisLocks m_lockPosition;
    AxisLocks m_lockRotation;
    bool m_startAsleep = false;
};

} // namespace yqlora

YQLORA_ENUM_NAMES(yqlora::BodyMotion, "Static", "Kinematic", "Dynamic")
YQLORA_ENUM_NAMES(yqlora::ForceMode, "Force", "Acceleration", "Impulse", "VelocityChange")

YQLORA_RECORD(
    yqlora::AxisLocks,
    field("x", &yqlora::AxisLocks::x),
    field("y", &yqlora::AxisLocks::y),
    field("z", &yqlora::AxisLocks::z)
)
