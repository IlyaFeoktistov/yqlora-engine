#pragma once

#include "yqlora/physics/Collider.hpp"
#include "yqlora/reflection/Property.hpp"

namespace yqlora {

// Выпуклая оболочка меша: форма для движущихся тел сложной формы (камень, бочка). Вогнутые места заполняются.
class YQLORA_API ConvexCollider final : public Collider {
public:
    YQLORA_TYPE(ConvexCollider, Collider)

    // Меш из файла или встроенный; пусто - коллайдер ни с чем не сталкивается.
    ResourceRef mesh() const
    {
        return m_mesh;
    }

    void setMesh(ResourceRef mesh);

    ColliderShape shape() const override;

private:
    ResourceRef m_mesh;
};

} // namespace yqlora
