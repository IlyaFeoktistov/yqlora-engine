#pragma once

#include "yqlora/physics/Collider.hpp"

namespace yqlora {

// Ящик: стороны size, как у встроенного куба при size 1.
class YQLORA_API BoxCollider final : public Collider {
public:
    YQLORA_TYPE(BoxCollider, Collider)

    const Vec3& size() const
    {
        return m_size;
    }

    // Каждая сторона не меньше kMinColliderSize.
    void setSize(const Vec3& size);

    ColliderShape shape() const override;

private:
    Vec3 m_size{1.0f};
};

} // namespace yqlora
