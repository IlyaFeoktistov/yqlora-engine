#pragma once

#include "yqlora/render/FrameData.hpp"
#include "yqlora/render/UniformRing.hpp"
#include "yqlora/rhi/Device.hpp"

#include <memory>

namespace yqlora {
class GpuMeshCache;
class ShaderCompiler;
} // namespace yqlora

namespace yqlora::opengl {

// Контур выбранного узла в редакторе. Сначала узел рисуется белым в маску
// размером с цель, затем полноэкранный проход закрашивает пиксели рядом
// с маской. Толщина контура в пикселях не зависит от расстояния до объекта,
// а сам контур виден и сквозь другие объекты.
class OutlinePass {
public:
    OutlinePass(rhi::Device& device, ShaderCompiler& shaders, GpuMeshCache& meshes, UniformRing& uniforms);

    // Свои проходы: маска, затем контур поверх color. Вызывается вне прохода.
    void draw(
        rhi::CommandList& commands,
        const FrameData& frame,
        const UniformSlice& viewUniforms,
        rhi::Texture& color,
        rhi::Texture* depth
    );

private:
    void resizeMask(int width, int height);

    rhi::Device& m_device;
    GpuMeshCache& m_meshes;
    UniformRing& m_uniforms;
    std::unique_ptr<rhi::Texture> m_mask;
    std::unique_ptr<rhi::Sampler> m_maskSampler;
    std::unique_ptr<rhi::Shader> m_maskShader;
    std::unique_ptr<rhi::Shader> m_outlineShader;
    std::unique_ptr<rhi::Pipeline> m_maskPipeline;
    std::unique_ptr<rhi::Pipeline> m_outlinePipeline;
};

} // namespace yqlora::opengl
