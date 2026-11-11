#pragma once

#include "yqlora/physics/Collider.hpp"

namespace yqlora {

// Капсула вдоль оси Y: радиус radius, высота height вместе с полусферами,
// как встроенная капсула при 0.5 и 2. Обычная фигура игрока.
class YQLORA_API CapsuleCollider final : public Collider {
public:
    YQLORA_TYPE(CapsuleCollider, Collider)

    float radius() const
    {
        return m_radius;
    }

    float height() const
    {
        return m_height;
    }

    // Не меньше kMinColliderSize. Высота меньше двух радиусов - шар радиуса radius.
    void setRadius(float radius);
    void setHeight(float height);

    ColliderShape shape() const override;

private:
    float m_radius = 0.5f;
    float m_height = 2.0f;
};

} // namespace yqlora
