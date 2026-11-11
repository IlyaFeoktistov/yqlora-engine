#pragma once

#include "yqlora/physics/Collider.hpp"

namespace yqlora {

// Цилиндр вдоль оси Y: радиус radius, высота height, как встроенный при 0.5 и 2.
class YQLORA_API CylinderCollider final : public Collider {
public:
    YQLORA_TYPE(CylinderCollider, Collider)

    float radius() const
    {
        return m_radius;
    }

    float height() const
    {
        return m_height;
    }

    // Не меньше kMinColliderSize.
    void setRadius(float radius);
    void setHeight(float height);

    ColliderShape shape() const override;

private:
    float m_radius = 0.5f;
    float m_height = 2.0f;
};

} // namespace yqlora
