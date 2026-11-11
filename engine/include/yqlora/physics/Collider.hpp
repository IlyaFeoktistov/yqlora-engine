#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/math/Math.hpp"
#include "yqlora/physics/ColliderShape.hpp"
#include "yqlora/scene/Component.hpp"

namespace yqlora {

// Самый малый размер фигуры коллайдера, м: меньше физика считает
// ненадёжно, а ноль - вырожденная фигура.
inline constexpr float kMinColliderSize = 0.001f;

// Форма для столкновений на узле Node3D. Тело из коллайдеров своего узла и
// потомков до ближайшего узла со своим RigidBody собирает RigidBody выше по
// дереву; коллайдер без RigidBody над ним сам даёт неподвижное тело, поэтому
// стене достаточно коллайдера. Устройство - docs/architecture/physics.md.
class YQLORA_API Collider : public Component {
public:
    YQLORA_TYPE(Collider, Component)

    // Сдвиг и поворот фигуры от узла, в его осях.
    const Vec3& center() const
    {
        return m_center;
    }

    void setCenter(const Vec3& center);

    // Углы Эйлера в градусах, как у Transform3D.
    const Vec3& rotationDegrees() const
    {
        return m_rotationDegrees;
    }

    void setRotationDegrees(const Vec3& degrees);

    Quat rotation() const;

    // Фигура в собственных осях коллайдера.
    virtual ColliderShape shape() const = 0;

protected:
    Collider() = default;

    // Состав тела изменился: физика перестроит его перед следующим шагом.
    void changed();

    void onEnterScene() override;
    void onExitScene() override;

private:
    Vec3 m_center{0.0f};
    Vec3 m_rotationDegrees{0.0f};
};

} // namespace yqlora
