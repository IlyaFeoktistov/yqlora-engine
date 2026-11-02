#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/render/FrameData.hpp"
#include "yqlora/scene/Scene.hpp"

namespace yqlora {

class EngineServices;

// Запущенная игра: её сцена и её доля кадра (см. frame.md). Позже здесь же
// смена сцен по запросу скрипта.
class YQLORA_API GameRuntime {
public:
    explicit GameRuntime(const EngineServices& services);

    Scene& scene()
    {
        return m_scene;
    }

    void simulate(float deltaSeconds);
    void updateTransforms();
    FrameData extract(float aspect) const;

private:
    Scene m_scene;
};

} // namespace yqlora
