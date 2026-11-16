#include "yqlora/runtime/GameRuntime.hpp"

#include "yqlora/debug/DebugDraw.hpp"
#include "yqlora/render/FrameBuilder.hpp"
#include "yqlora/render/FrameExtract.hpp"

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

FrameData GameRuntime::extract(float aspect)
{
    FrameData frame = extractFrame(m_scene, aspect);
    if (const DebugDraw* debug = m_scene.system<DebugDraw>()) {
        extractDebugLines(m_scene, debug->gameChannels(), frame);
    }

    return frame;
}

} // namespace yqlora
