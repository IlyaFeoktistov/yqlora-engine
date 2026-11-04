#pragma once

#include "yqlora/render/FrameData.hpp"
#include "yqlora/resource/ResourceId.hpp"
#include "yqlora/scene/Scene.hpp"

namespace yqlora {
class EngineServices;
class Environment;
class MeshRenderer;
class ResourceCache;
} // namespace yqlora

namespace yqlora::editor {

// Превью материала: своя сцена со сферой, камерой, солнцем и окружением по
// умолчанию (небо, его свет и отражения). Материал неба становится небом
// превью, а сфера - белой: видно и небо, и как оно освещает. Ни одна часть
// превью не в документе, и его свет сцену не освещает.
class MaterialPreview {
public:
    explicit MaterialPreview(const EngineServices& services);

    // Кадр превью материала material для цели с соотношением сторон aspect.
    FrameData frame(ResourceId material, float aspect);

private:
    Scene m_scene;
    ResourceCache& m_cache;
    MeshRenderer* m_sphere = nullptr;
    Environment* m_environment = nullptr;
};

} // namespace yqlora::editor
