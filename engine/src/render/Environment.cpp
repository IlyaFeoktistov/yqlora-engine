#include "yqlora/render/Environment.hpp"

#include "yqlora/render/RenderSystem.hpp"
#include "yqlora/scene/Scene.hpp"

#include <algorithm>

namespace yqlora {

Environment::Environment(std::string name)
    : Node(std::move(name))
{
}

void Environment::setAmbientIntensity(float intensity)
{
    m_ambientIntensity = std::max(intensity, 0.0f);
}

void Environment::setGradientLuminance(float luminance)
{
    m_gradientLuminance = std::max(luminance, 0.0f);
}

void Environment::onEnterScene()
{
    Node::onEnterScene();
    if (RenderSystem* render = scene()->system<RenderSystem>()) {
        render->addEnvironment(*this);
    }
}

void Environment::onExitScene()
{
    if (RenderSystem* render = scene()->system<RenderSystem>()) {
        render->removeEnvironment(*this);
    }
    Node::onExitScene();
}

} // namespace yqlora
