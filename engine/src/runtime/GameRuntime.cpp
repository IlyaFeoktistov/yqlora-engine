#include "yqlora/runtime/GameRuntime.hpp"

#include "yqlora/render/FrameBuilder.hpp"

namespace yqlora {

GameRuntime::GameRuntime(const EngineServices& services)
    : m_scene(services)
{
    m_scene.setPlaying(true);
}

void GameRuntime::simulate(float deltaSeconds)
{
    m_scene.advance(deltaSeconds);
}

void GameRuntime::updateTransforms()
{
    m_scene.updateTransforms();
}

FrameData GameRuntime::extract(float aspect) const
{
    return extractFrame(m_scene, aspect);
}

} // namespace yqlora
