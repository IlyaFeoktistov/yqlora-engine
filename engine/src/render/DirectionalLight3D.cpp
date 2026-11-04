#include "yqlora/render/DirectionalLight3D.hpp"

namespace yqlora {

namespace {

// Освещённость в люксах: солнце ясным днём.
constexpr float kDefaultIntensity = 100000.0f;

} // namespace

DirectionalLight3D::DirectionalLight3D(std::string name)
    : Light3D(std::move(name), LightKind::Directional, kDefaultIntensity)
{
}

} // namespace yqlora
