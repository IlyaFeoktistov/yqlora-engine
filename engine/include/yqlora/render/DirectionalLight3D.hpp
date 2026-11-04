#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/render/Light3D.hpp"

namespace yqlora {

// Свет издалека, одинаковый во всей сцене: важен только поворот узла.
class YQLORA_API DirectionalLight3D final : public Light3D {
public:
    explicit DirectionalLight3D(std::string name = "DirectionalLight3D");

    YQLORA_TYPE(DirectionalLight3D, Light3D)

    // Солнце сцены: по нему небо, атмосфера ослабляет его свет. Если флажка
    // нет ни у одного включённого, солнце - самый яркий направленный.
    bool sun() const
    {
        return m_sun;
    }

    void setSun(bool sun)
    {
        m_sun = sun;
    }

private:
    bool m_sun = false;
};

} // namespace yqlora
