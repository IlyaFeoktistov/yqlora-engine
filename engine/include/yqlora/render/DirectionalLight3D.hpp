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

    // Угловой диаметр источника в градусах: у солнца 0.53. По нему ширина
    // полутени (ShadowMode::Soft) и диск солнца на небе. Не меньше нуля.
    float angularDiameter() const
    {
        return m_angularDiameter;
    }

    void setAngularDiameter(float degrees);

private:
    bool m_sun = false;
    float m_angularDiameter = 0.53f;
};

} // namespace yqlora
