#include "yqlora/render/PointLight3D.hpp"

#include <algorithm>

namespace yqlora {

namespace {

// Сила света в канделах: лампа накаливания 100 Вт.
constexpr float kDefaultIntensity = 130.0f;

// При нулевой дальности окно затухания делит на ноль.
constexpr float kMinRange = 0.01f;

} // namespace

PointLight3D::PointLight3D(std::string name)
    : PointLight3D(std::move(name), LightKind::Point)
{
}

PointLight3D::PointLight3D(std::string name, LightKind kind)
    : Light3D(std::move(name), kind, kDefaultIntensity)
{
}

void PointLight3D::setRange(float range)
{
    m_range = std::max(range, kMinRange);
}

} // namespace yqlora
