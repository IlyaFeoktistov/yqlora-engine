#pragma once

#include "yqlora/physics/Collider.hpp"

namespace yqlora {

// Шар радиуса radius, как встроенная сфера при 0.5.
class YQLORA_API SphereCollider final : public Collider {
public:
    YQLORA_TYPE(SphereCollider, Collider)

    float radius() const
    {
        return m_radius;
    }

    // Не меньше kMinColliderSize.
    void setRadius(float radius);

    ColliderShape shape() const override;

private:
    float m_radius = 0.5f;
};

} // namespace yqlora
