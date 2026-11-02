#pragma once

#include "yqlora/rhi/Device.hpp"

#include <map>
#include <memory>
#include <utility>

// Намеренно без glad/SDL: заголовок включают части бэкенда без них.
struct SDL_GLContextState;

namespace yqlora {
class Window;
}

namespace yqlora::opengl {

class GlCommandList;
class GlTexture;

// rhi::Device на OpenGL 3.3 core. Команды исполняются сразу, кадров в полёте
// нет, поэтому удалённые объекты освобождаются сразу.
class GlDevice final : public rhi::Device {
public:
    static constexpr int kVersionMajor = 3;
    static constexpr int kVersionMinor = 3;

    // Атрибуты контекста (версия, core profile, буферы), задаются до создания окна.
    static void prepareWindow();

    explicit GlDevice(Window& window);
    ~GlDevice() override;

    GlDevice(const GlDevice&) = delete;
    GlDevice& operator=(const GlDevice&) = delete;

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

    rhi::Texture& backBuffer() override;
    rhi::Texture& backBufferDepth() override;
    void resizeBackBuffer(int width, int height) override;

    rhi::CommandList& commands() override;
    void present() override;
    bool setVsync(bool enabled) override;

    SDL_GLContextState* context() const
    {
        return m_context;
    }

    // Framebuffer для пары вложений; заводится при первом обращении.
    unsigned int framebuffer(const GlTexture& color, const GlTexture* depth);

    // Текстура удаляется: её framebuffer больше не нужны.
    void forgetFramebuffers(unsigned int texture);

private:
    Window& m_window;
    SDL_GLContextState* m_context = nullptr;
    rhi::DeviceLimits m_limits;
    std::unique_ptr<GlTexture> m_backBuffer;
    std::unique_ptr<GlTexture> m_backBufferDepth;
    std::unique_ptr<GlCommandList> m_commands;
    // Ключ - имена текстур цвета и глубины (0 - без глубины).
    std::map<std::pair<unsigned int, unsigned int>, unsigned int> m_framebuffers;
};

} // namespace yqlora::opengl
