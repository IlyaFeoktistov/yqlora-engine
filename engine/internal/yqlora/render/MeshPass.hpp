#pragma once

#include "yqlora/render/FrameData.hpp"
#include "yqlora/render/UniformRing.hpp"
#include "yqlora/render/ViewPass.hpp"
#include "yqlora/rhi/Device.hpp"

#include <memory>
#include <span>

namespace yqlora {

// Непрозрачные меши вида шейдером Standard, в HDR-цель кадра.
class MeshPass {
public:
    explicit MeshPass(const RenderContext& context);

    // Меши в открытый проход, в порядке visible.
    void draw(
        rhi::CommandList& commands,
        std::span<const DrawItem* const> visible,
        const UniformSlice& frameUniforms,
        const UniformSlice& viewUniforms
    ) const;

private:
    GpuMeshCache& m_meshes;
    UniformRing& m_uniforms;
    std::unique_ptr<rhi::Shader> m_shader;
    std::unique_ptr<rhi::Pipeline> m_pipeline;
};

} // namespace yqlora
