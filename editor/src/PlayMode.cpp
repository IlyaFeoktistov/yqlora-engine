#include "yqlora/editor/PlayMode.hpp"

#include "yqlora/core/EngineServices.hpp"
#include "yqlora/core/Log.hpp"
#include "yqlora/scene/Scene.hpp"
#include "yqlora/scene/SceneSerializer.hpp"

namespace yqlora::editor {

PlayMode::PlayMode(const SceneSerializer& serializer)
    : m_serializer(serializer)
{
}

PlayMode::~PlayMode() = default;

bool PlayMode::start(const Scene& source)
{
    if (playing()) {
        return false;
    }
    m_input = Input{};
    m_services = std::make_unique<EngineServices>(m_input, source.services());
    // Копия через формат файла: в ней те же ID узлов, и выделение остаётся на месте.
    auto copy = std::make_unique<Scene>(*m_services);
    copy->setPlaying(true);
    if (!m_serializer.fromJson(*copy, m_serializer.toJson(source))) {
        log::error("Cannot copy the scene for Play");
        m_services.reset();

        return false;
    }
    m_scene = std::move(copy);
    m_paused = false;

    return true;
}

void PlayMode::stop()
{
    if (!m_scene) {
        return;
    }
    m_scene->stop();
    m_scene.reset();
    m_services.reset();
    m_paused = false;
}

void PlayMode::setPaused(bool paused)
{
    m_paused = playing() && paused;
}

void PlayMode::update(float deltaSeconds, const Input& input)
{
    m_input = m_inputFocused ? input : Input{};
    if (running()) {
        m_scene->advance(deltaSeconds);
    }
    if (playing()) {
        // На паузе узлы тоже двигают: в инспекторе и манипулятором.
        m_scene->updateTransforms();
    }
}

} // namespace yqlora::editor
