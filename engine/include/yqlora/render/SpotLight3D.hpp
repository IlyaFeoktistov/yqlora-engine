#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/render/PointLight3D.hpp"

namespace yqlora {

// Точечный свет, ограниченный конусом вдоль -Z. Внутри внутреннего угла
// светит в полную силу, к внешнему плавно гаснет. Углы - от оси конуса, в
// градусах.
class YQLORA_API SpotLight3D final : public PointLight3D {
public:
    explicit SpotLight3D(std::string name = "SpotLight3D");

    YQLORA_TYPE(SpotLight3D, PointLight3D)

    float innerAngleDegrees() const
    {
        return m_innerAngle;
    }

    float outerAngleDegrees() const
    {
        return m_outerAngle;
    }

    // Внутренний угол не больше внешнего: при конфликте сдвигается другой угол.
    void setInnerAngleDegrees(float angle);
    void setOuterAngleDegrees(float angle);

private:
    float m_innerAngle = 20.0f;
    float m_outerAngle = 30.0f;
};

} // namespace yqlora
