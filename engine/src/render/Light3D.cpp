#include "yqlora/render/Light3D.hpp"

#include "yqlora/render/RenderSystem.hpp"
#include "yqlora/scene/Scene.hpp"

#include <algorithm>

namespace yqlora {

Light3D::Light3D(std::string name, LightKind kind, float intensity)
    : Node3D(std::move(name))
    , m_kind(kind)
    , m_intensity(intensity)
{
}

void Light3D::setIntensity(float intensity)
{
    m_intensity = std::max(intensity, 0.0f);
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
