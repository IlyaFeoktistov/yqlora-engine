#pragma once

#include "yqlora/render/FrameData.hpp"
#include "yqlora/resource/ResourceId.hpp"
#include "yqlora/scene/Scene.hpp"

namespace yqlora {
class EngineServices;
class MeshRenderer;
} // namespace yqlora

namespace yqlora::editor {

// Превью материала: своя сцена со сферой, камерой и постоянным направленным
// светом. Ни одна её часть не в документе, и её свет сцену не освещает.
class MaterialPreview {
public:
    explicit MaterialPreview(const EngineServices& services);

    // Кадр превью материала material для цели с соотношением сторон aspect.
    FrameData frame(ResourceId material, float aspect);

private:
    Scene m_scene;
    MeshRenderer* m_sphere = nullptr;
};

} // namespace yqlora::editor
