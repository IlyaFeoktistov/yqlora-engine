#include "yqlora/render/SpotLight3D.hpp"

#include <algorithm>

namespace yqlora {

namespace {

// Конус шире полусферы уже не конус, а при 90 градусах вырождается.
constexpr float kMaxAngleDegrees = 89.0f;

} // namespace

SpotLight3D::SpotLight3D(std::string name)
    : PointLight3D(std::move(name), LightKind::Spot)
{
}

void SpotLight3D::setInnerAngleDegrees(float angle)
{
    m_innerAngle = std::clamp(angle, 0.0f, kMaxAngleDegrees);
    m_outerAngle = std::max(m_outerAngle, m_innerAngle);
}

void SpotLight3D::setOuterAngleDegrees(float angle)
{
    m_outerAngle = std::clamp(angle, 0.0f, kMaxAngleDegrees);
    m_innerAngle = std::min(m_innerAngle, m_outerAngle);
}

} // namespace yqlora
