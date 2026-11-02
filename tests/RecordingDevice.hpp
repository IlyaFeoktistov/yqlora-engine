#pragma once

#include <yqlora/rhi/Device.hpp>

#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

// rhi::Device без видеокарты: записывает команды и проверяет контракты RHI
// (порядок вызовов, состояния текстур, форматы конвейера и цели, привязки).
// Нарушение - std::logic_error, как и в настоящих бэкендах.
namespace yqlora::test {

enum class CommandKind {
    Transition,
    BeginPass,
    EndPass,
    BindPipeline,
    SetVertexBuffer,
    SetIndexBuffer,
    SetUniformBuffer,
    SetTexture,
    Draw,
    DrawIndexed,
    Present,
};

struct Command {
    CommandKind kind;
    // Число вершин или индексов у Draw, смещение у SetUniformBuffer, новое
    // состояние у Transition.
    std::uint64_t value = 0;
    // Шейдер конвейера у BindPipeline.
    std::string shader{};
};

class RecordingBuffer final : public rhi::Buffer {
public:
    RecordingBuffer(const rhi::BufferDesc& desc, std::span<const std::byte> data);

    void write(std::size_t offset, std::span<const std::byte> data) override;

    std::vector<std::byte> bytes;
};

class RecordingTexture final : public rhi::Texture {
public:
    using rhi::Texture::Texture;

    void resize(int width, int height)
    {
        setSize(width, height);
    }
};

class RecordingDevice;

class RecordingCommandList final : public rhi::CommandList {
public:
    explicit RecordingCommandList(RecordingDevice& device)
        : m_device(device)
    {
    }

    void beginRenderPass(const rhi::RenderPassDesc& desc) override;
    void endRenderPass() override;
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
    void draw(std::uint32_t vertexCount, std::uint32_t firstVertex) override;
    void drawIndexed(std::uint32_t indexCount, std::uint32_t firstIndex) override;

    bool inPass() const
    {
        return m_inPass;
    }

protected:
    void recordTransition(rhi::Texture& texture, rhi::ResourceState from, rhi::ResourceState to) override;

private:
    const rhi::ShaderBinding& binding(std::uint32_t set, std::uint32_t binding, rhi::BindingKind kind) const;
    void checkDrawable() const;

    RecordingDevice& m_device;
    bool m_inPass = false;
    rhi::TargetFormats m_target;
    const rhi::Pipeline* m_pipeline = nullptr;
    std::set<std::pair<std::uint32_t, std::uint32_t>> m_bound;
    bool m_hasVertexBuffer = false;
    bool m_hasIndexBuffer = false;
};

class RecordingDevice final : public rhi::Device {
public:
    static constexpr int kBackBufferSize = 64;

    RecordingDevice();

    const rhi::DeviceLimits& limits() const override
    {
        return m_limits;
    }

    std::unique_ptr<rhi::Buffer> createBuffer(const rhi::BufferDesc& desc, std::span<const std::byte> data) override;
    std::unique_ptr<rhi::Texture> createTexture(
        const rhi::TextureDesc& desc,
        std::span<const std::byte> pixels
    ) override;
    std::unique_ptr<rhi::Sampler> createSampler(const rhi::SamplerDesc& desc) override;
    std::unique_ptr<rhi::Shader> createShader(rhi::ShaderDesc desc) override;
    std::unique_ptr<rhi::Pipeline> createPipeline(rhi::PipelineDesc desc) override;

    rhi::Texture& backBuffer() override
    {
        return m_backBuffer;
    }

    rhi::Texture& backBufferDepth() override
    {
        return m_backBufferDepth;
    }

    void resizeBackBuffer(int width, int height) override;

    rhi::CommandList& commands() override
    {
        return m_commands;
    }

    void present() override;

    bool setVsync(bool) override
    {
        return true;
    }

    // Записанные команды всех кадров.
    std::vector<Command> recorded;

    std::vector<CommandKind> kinds() const;

private:
    rhi::DeviceLimits m_limits;
    RecordingTexture m_backBuffer;
    RecordingTexture m_backBufferDepth;
    RecordingCommandList m_commands;
};

} // namespace yqlora::test
