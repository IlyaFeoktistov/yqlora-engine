#pragma once

#include "yqlora/rhi/CommandList.hpp"
#include "yqlora/rhi/Pipeline.hpp"

#include <cstdint>

namespace yqlora::opengl {

class GlBuffer;
class GlDevice;
class GlShader;

// Команды OpenGL исполняются сразу, при вызове. Состояние GL выставляется
// целиком при каждом bindPipeline: между проходами его меняет ImGui.
class GlCommandList final : public rhi::CommandList {
public:
    explicit GlCommandList(GlDevice& device);
    ~GlCommandList() override;

    GlCommandList(const GlCommandList&) = delete;
    GlCommandList& operator=(const GlCommandList&) = delete;

    void beginRenderPass(const rhi::RenderPassDesc& desc) override;
    void endRenderPass() override;
    void setViewport(const rhi::Viewport& viewport) override;
    void bindPipeline(const rhi::Pipeline& pipeline) override;
    void setVertexBuffer(const rhi::Buffer& buffer) override;
    void setIndexBuffer(const rhi::Buffer& buffer) override;
    void setUniformBuffer(
        std::uint32_t set,
        std::uint32_t binding,
        const rhi::Buffer& buffer,
        std::size_t offset,
        std::size_t size
    ) override;
    void setTexture(
        std::uint32_t set,
        std::uint32_t binding,
        const rhi::Texture& texture,
        const rhi::Sampler& sampler
    ) override;
    void generateMipmaps(rhi::Texture& texture) override;
    void draw(std::uint32_t vertexCount, std::uint32_t firstVertex) override;
    void drawIndexed(std::uint32_t indexCount, std::uint32_t firstIndex) override;

protected:
    // Переходы в OpenGL делает драйвер.
    void recordTransition(rhi::Texture&, rhi::ResourceState, rhi::ResourceState) override
    {
    }

private:
    const GlShader& shader() const;
    unsigned int slot(std::uint32_t set, std::uint32_t binding) const;
    // Атрибуты вершин по раскладке конвейера из текущего вершинного буфера.
    void applyVertexLayout();

    GlDevice& m_device;
    unsigned int m_vertexArray = 0;
    bool m_inPass = false;
    rhi::TargetFormats m_target;
    const rhi::Pipeline* m_pipeline = nullptr;
    const GlBuffer* m_vertexBuffer = nullptr;
    bool m_layoutDirty = true;
    unsigned int m_enabledAttributes = 0;
};

} // namespace yqlora::opengl
