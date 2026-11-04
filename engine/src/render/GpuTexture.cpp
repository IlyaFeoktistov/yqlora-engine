#include "yqlora/render/GpuTexture.hpp"

#include "yqlora/render/TextureData.hpp"
#include "yqlora/render/TextureSampler.hpp"

#include <cstdint>
#include <span>

namespace yqlora {

namespace {

// Пиксели текстур по умолчанию, RGBA8, по порядку DefaultTexture.
constexpr std::uint8_t kChannelMax = 255;
constexpr std::uint8_t kChannelHalf = 128;
constexpr std::array<std::array<std::uint8_t, 4>, std::size(EnumNames<DefaultTexture>::names)> kDefaultPixels{{
    {kChannelMax, kChannelMax, kChannelMax, kChannelMax},
    {0, 0, 0, kChannelMax},
    // Нормаль (0, 0, 1) в касательном пространстве.
    {kChannelHalf, kChannelHalf, kChannelMax, kChannelMax},
}};

} // namespace

rhi::Format gpuTextureFormat(const TextureData& texture)
{
    if (texture.format == PixelFormat::Rgba16Float) {
        return rhi::Format::Rgba16Float;
    }

    return texture.sampling.colorSpace == ColorSpace::Srgb ? rhi::Format::Rgba8Srgb : rhi::Format::Rgba8;
}

GpuTextureCache::GpuTextureCache(rhi::Device& device)
    : m_device(device)
{
    for (std::size_t i = 0; i < m_defaults.size(); ++i) {
        // Данные, а не цвет: декодировать из sRGB нечего.
        m_defaults[i] = device.createTexture(
            {.format = rhi::Format::Rgba8},
            std::as_bytes(std::span(kDefaultPixels[i]))
        );
    }
}

const rhi::Texture& GpuTextureCache::texture(const FrameTexture& texture)
{
    if (!texture.data) {
        return *m_defaults[static_cast<std::size_t>(texture.fallback)];
    }
    const TextureData& data = *texture.data;

    return m_textures.get(texture.id, texture.revision, [&] {
        return m_device.createTexture(
            {.width = data.width,
             .height = data.height,
             .format = gpuTextureFormat(data),
             .mipmaps = data.sampling.mipmaps},
            std::as_bytes(std::span(data.pixels))
        );
    });
}

const rhi::Sampler& GpuTextureCache::sampler(const FrameTexture& texture)
{
    const rhi::SamplerDesc desc = texture.data ? samplerDesc(texture.data->sampling) : rhi::SamplerDesc{};
    std::unique_ptr<rhi::Sampler>& sampler = m_samplers[{desc.filter, desc.wrap, desc.mipmaps}];
    if (!sampler) {
        sampler = m_device.createSampler(desc);
    }

    return *sampler;
}

} // namespace yqlora
