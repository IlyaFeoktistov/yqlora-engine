#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/reflection/EnumNames.hpp"
#include "yqlora/scene/Component.hpp"

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
};

} // namespace yqlora

YQLORA_ENUM_NAMES(yqlora::BodyMotion, "Static", "Kinematic", "Dynamic")
