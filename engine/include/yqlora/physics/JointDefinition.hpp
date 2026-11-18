#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/math/Math.hpp"
#include "yqlora/physics/AxisLocks.hpp"
#include "yqlora/reflection/EnumNames.hpp"
#include "yqlora/reflection/Record.hpp"

#include <cstdint>
#include <variant>

namespace yqlora {

// Пределы движения скрепления вдоль или вокруг его оси: градусы у петли,
// метры у слайдера. Выключены - движение свободное.
struct JointLimits {
    bool enabled = false;
    float min = 0.0f;
    float max = 0.0f;

    bool operator==(const JointLimits&) const = default;
};

// Мягкость: частота пружины, Гц (0 - жёстко), и затухание (1 - без
// колебаний). Частота, а не жёсткость, поэтому от масс тел не зависит.
struct JointSpring {
    float frequency = 0.0f;
    float damping = 0.0f;

    bool operator==(const JointSpring&) const = default;
};

// Мотор скрепления с осью: Velocity держит скорость (градусы в секунду у
// петли, м/с у слайдера), Position ведёт к положению (градусы, метры).
enum class JointMotorMode : std::uint8_t {
    Off,
    Velocity,
    Position,
};

struct JointMotor {
    JointMotorMode mode = JointMotorMode::Off;
    float target = 0.0f;
    // Предельная сила, Н, или момент, Н*м, мотора.
    float maxForce = 0.0f;

    bool operator==(const JointMotor&) const = default;
};

// Что скрепление описывает, без типов физической библиотеки: из этого
// строит ограничение физика движка. Ось и нормаль к ней - в осях узла
// скрепления.
struct FixedJointDefinition {
    bool operator==(const FixedJointDefinition&) const = default;
};

struct PointJointDefinition {
    bool operator==(const PointJointDefinition&) const = default;
};

struct HingeJointDefinition {
    Vec3 axis{0.0f, 1.0f, 0.0f};
    JointLimits limits;
    JointSpring limitSpring;
    JointMotor motor;

    bool operator==(const HingeJointDefinition&) const = default;
};

struct SliderJointDefinition {
    Vec3 axis{1.0f, 0.0f, 0.0f};
    JointLimits limits;
    JointSpring limitSpring;
    JointMotor motor;

    bool operator==(const SliderJointDefinition&) const = default;
};

// Расстояние между точками крепления от min до max, м; оба меньше нуля -
// то, что было при создании. Точка на другом теле - connectedAnchor в осях
// его узла (у мира - точка мира).
struct DistanceJointDefinition {
    Vec3 connectedAnchor{0.0f};
    float minDistance = -1.0f;
    float maxDistance = -1.0f;
    JointSpring spring;

    bool operator==(const DistanceJointDefinition&) const = default;
};

// Качание внутри конуса вокруг оси; кручение свободно.
struct ConeJointDefinition {
    Vec3 axis{0.0f, 1.0f, 0.0f};
    // Полуугол конуса, градусы.
    float halfConeAngle = 45.0f;

    bool operator==(const ConeJointDefinition&) const = default;
};

// Качание и кручение вокруг оси (кручения) в пределах, градусы: качание -
// полуугол в плоскости нормали (normal) и поперёк неё, кручение - от min до
// max. Мотор (Velocity, Position) ведёт углы motorTarget: кручение (x),
// качание вокруг нормали (y) и поперёк (z).
struct SwingTwistJointDefinition {
    Vec3 axis{0.0f, 1.0f, 0.0f};
    float normalHalfConeAngle = 45.0f;
    float planeHalfConeAngle = 45.0f;
    float twistMin = -45.0f;
    float twistMax = 45.0f;
    // Трение в суставе без мотора, Н*м.
    float maxFrictionTorque = 0.0f;
    JointMotorMode motor = JointMotorMode::Off;
    Vec3 motorTarget{0.0f};
    float maxMotorTorque = 0.0f;

    bool operator==(const SwingTwistJointDefinition&) const = default;
};

// Шесть осей узла скрепления: запертые не движутся, с включёнными пределами
// движутся в них (метры, градусы), остальные свободны. Пружина - мягкость
// пределов сдвига.
struct SixDofJointDefinition {
    AxisLocks lockLinear;
    AxisLocks lockAngular;
    JointLimits linearX;
    JointLimits linearY;
    JointLimits linearZ;
    JointLimits angularX;
    JointLimits angularY;
    JointLimits angularZ;
    JointSpring linearSpring;

    bool operator==(const SixDofJointDefinition&) const = default;
};

using JointDefinition = std::variant<
    FixedJointDefinition,
    PointJointDefinition,
    HingeJointDefinition,
    SliderJointDefinition,
    DistanceJointDefinition,
    ConeJointDefinition,
    SwingTwistJointDefinition,
    SixDofJointDefinition>;

// Ось скрепления в осях его узла, единичная: ось вида, у которого она
// есть, у остальных - ось Y узла. По ней физика строит ограничение, а
// редактор рисует скрепление.
YQLORA_API Vec3 jointAxis(const JointDefinition& definition);

// Нуль угла петли и вторая ось рамки скрепления: единичный перпендикуляр к
// единичной axis, всегда один и тот же для одной оси.
YQLORA_API Vec3 jointNormal(const Vec3& axis);

} // namespace yqlora

YQLORA_RECORD(
    yqlora::JointLimits,
    field("enabled", &yqlora::JointLimits::enabled),
    field("min", &yqlora::JointLimits::min),
    field("max", &yqlora::JointLimits::max)
)

YQLORA_RECORD(
    yqlora::JointSpring,
    field("frequency", &yqlora::JointSpring::frequency),
    field("damping", &yqlora::JointSpring::damping)
)

YQLORA_ENUM_NAMES(yqlora::JointMotorMode, "Off", "Velocity", "Position")
