#pragma once

#include "yqlora/rhi/Texture.hpp"

#include <cstddef>
#include <memory>
#include <span>

namespace yqlora::opengl {

class GlDevice;

// Текстура OpenGL. Задний буфер окна - тоже GlTexture, но без своего имени:
// он рисуется через framebuffer 0.
class GlTexture final : public rhi::Texture {
public:
    GlTexture(GlDevice& device, const rhi::TextureDesc& desc, std::span<const std::byte> pixels);
    ~GlTexture() override;

    // Задний буфер окна (цвет или глубина).
    static std::unique_ptr<GlTexture> backBuffer(GlDevice& device, const rhi::TextureDesc& desc);

    unsigned int id() const
    {
        return m_texture;
    }

    // GL_TEXTURE_2D или GL_TEXTURE_CUBE_MAP.
    unsigned int target() const;

    bool isBackBuffer() const
    {
        return m_texture == 0;
    }

    void resizeBackBuffer(int width, int height)
    {
        setSize(width, height);
    }

    // Фильтр и повтор самой текстуры. Сцена читает через rhi::Sampler, а ImGui
    // сэмплеров не знает и берёт эти, поэтому превью в интерфейсе их задаёт.
    void setOwnSampling(const rhi::SamplerDesc& sampling);

private:
    struct BackBufferTag {};
    GlTexture(GlDevice& device, const rhi::TextureDesc& desc, BackBufferTag);

    GlDevice& m_device;
    unsigned int m_texture = 0;
};

// Сэмплер OpenGL (sampler object).
class GlSampler final : public rhi::Sampler {
public:
    explicit GlSampler(const rhi::SamplerDesc& desc);
    ~GlSampler() override;

    unsigned int id() const
    {
        return m_sampler;
    }

private:
    unsigned int m_sampler = 0;
};

} // namespace yqlora::opengl
