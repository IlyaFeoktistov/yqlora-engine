#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/render/FrameData.hpp"
#include "yqlora/render/GpuResourceCache.hpp"
#include "yqlora/render/TextureData.hpp"
#include "yqlora/rhi/Device.hpp"

#include <array>
#include <map>
#include <memory>
#include <tuple>

namespace yqlora {

// Текстуры материалов в видеопамяти: копии ресурсов по ID и ревизии (см.
// GpuResourceCache), текстуры по умолчанию и сэмплеры. Текстура,
// импортированная как sRGB, становится Rgba8Srgb: чтение в шейдере её
// декодирует в линейный цвет.
class YQLORA_API GpuTextureCache {
public:
    explicit GpuTextureCache(rhi::Device& device);

    // Текстура ресурса, а без данных - текстура по умолчанию поля.
    const rhi::Texture& texture(const FrameTexture& texture);
    // Сэмплер по настройкам импорта текстуры; у текстуры по умолчанию - обычный.
    const rhi::Sampler& sampler(const FrameTexture& texture);

    void endFrame()
    {
        m_textures.endFrame();
    }

private:
    using SamplerKey = std::tuple<rhi::Filter, rhi::Wrap, bool>;

    rhi::Device& m_device;
    GpuResourceCache<rhi::Texture> m_textures;
    // 1 x 1 по DefaultTexture.
    std::array<std::unique_ptr<rhi::Texture>, std::size(EnumNames<DefaultTexture>::names)> m_defaults;
    std::map<SamplerKey, std::unique_ptr<rhi::Sampler>> m_samplers;
};

// Формат текстуры в видеопамяти по её настройкам импорта.
YQLORA_API rhi::Format gpuTextureFormat(const TextureSampling& sampling);

} // namespace yqlora
