#pragma once

#include "yqlora/physics/Collider.hpp"
#include "yqlora/reflection/Property.hpp"

namespace yqlora {

// Треугольники меша: точная форма пола, стен, ландшафта. Только у неподвижных и кинематических тел (так требует Jolt): у динамического тела вместо них берётся выпуклая оболочка, а лог говорит об этом один раз.
class YQLORA_API MeshCollider final : public Collider {
public:
    YQLORA_TYPE(MeshCollider, Collider)

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
