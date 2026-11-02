#pragma once

#include "yqlora/render/FrameData.hpp"
#include "yqlora/render/UniformRing.hpp"
#include "yqlora/rhi/Device.hpp"

#include <memory>

namespace yqlora {
class GpuMeshCache;
}

namespace yqlora::opengl {

// Непрозрачные меши кадра. Пока источников света в сцене нет, всё освещено
// одним фиксированным направленным светом из данных кадра.
class MeshPass {
public:
    MeshPass(rhi::Device& device, GpuMeshCache& meshes, UniformRing& uniforms);

    // Меши кадра со слоем из маски вида, в открытый проход.
    void draw(
        rhi::CommandList& commands,
        const FrameData& frame,
        const ViewData& view,
        const UniformSlice& frameUniforms,
        const UniformSlice& viewUniforms
    ) const;

private:
    GpuMeshCache& m_meshes;
    UniformRing& m_uniforms;
    std::unique_ptr<rhi::Shader> m_shader;
    std::unique_ptr<rhi::Pipeline> m_pipeline;
};

} // namespace yqlora::opengl
