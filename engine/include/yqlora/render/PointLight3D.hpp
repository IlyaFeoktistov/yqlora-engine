#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/render/Light3D.hpp"

namespace yqlora {

// Свет из точки во все стороны. Слабеет с квадратом расстояния и плавно
// гаснет до нуля к дальности.
class YQLORA_API PointLight3D : public Light3D {
public:
    explicit PointLight3D(std::string name = "PointLight3D");

    YQLORA_TYPE(PointLight3D, Light3D)

    // Дальше этого расстояния в метрах свет ничего не освещает.
    float range() const
    {
        return m_range;
    }

    void setRange(float range);

protected:
    // Для прожектора: он тоже светит из точки, только конусом.
    PointLight3D(std::string name, LightKind kind);

private:
    float m_range = 10.0f;
};

} // namespace yqlora
