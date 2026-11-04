#include "yqlora/render/Light3D.hpp"

#include "yqlora/render/RenderSystem.hpp"
#include "yqlora/scene/Scene.hpp"

#include <algorithm>

namespace yqlora {

Light3D::Light3D(std::string name, LightKind kind, float intensity, ShadowMode shadows)
    : Node3D(std::move(name))
    , m_kind(kind)
    , m_intensity(intensity)
    , m_shadows(shadows)
{
}

void Light3D::setIntensity(float intensity)
{
    m_intensity = std::max(intensity, 0.0f);
}

void Light3D::setShadowStrength(float strength)
{
    m_shadowStrength = std::clamp(strength, 0.0f, 1.0f);
}

void Light3D::setShadowSoftness(float softness)
{
    m_shadowSoftness = std::max(softness, 0.0f);
}

void Light3D::setShadowBias(float bias)
{
    m_shadowBias = std::max(bias, 0.0f);
}

void Light3D::setShadowNormalBias(float bias)
{
    m_shadowNormalBias = std::max(bias, 0.0f);
}

void Light3D::onEnterScene()
{
    Node3D::onEnterScene();
    if (RenderSystem* render = scene()->system<RenderSystem>()) {
        render->addLight(*this);
    }
}

void Light3D::onExitScene()
{
    if (RenderSystem* render = scene()->system<RenderSystem>()) {
        render->removeLight(*this);
    }
    Node3D::onExitScene();
}

} // namespace yqlora
