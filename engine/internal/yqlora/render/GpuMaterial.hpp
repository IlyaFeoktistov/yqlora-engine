#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/render/FrameData.hpp"
#include "yqlora/render/GpuResourceCache.hpp"
#include "yqlora/rhi/Device.hpp"

#include <cstdint>
#include <memory>

namespace yqlora {

// Блоки Material материалов в видеопамяти: uniform-буфер, записанный по
// раскладке шейдера (materialBlock), по ID материала и ревизиям материала и
// шейдера. Текстуры сюда не входят: их держит GpuTextureCache.
class YQLORA_API GpuMaterialCache {
public:
    explicit GpuMaterialCache(rhi::Device& device)
        : m_device(device)
    {
    }

    // nullptr, если у шейдера материала нет блока Material.
    const rhi::Buffer* block(const FrameMaterial& material);

    void endFrame()
    {
        m_blocks.endFrame();
    }

private:
    struct Revision {
        std::uint64_t material = 0;
        std::uint64_t shader = 0;

        bool operator==(const Revision&) const = default;
    };

    rhi::Device& m_device;
    GpuResourceCache<rhi::Buffer, Revision> m_blocks;
};

} // namespace yqlora
