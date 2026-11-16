#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/render/BufferRing.hpp"
#include "yqlora/rhi/Device.hpp"
#include "yqlora/rhi/Pipeline.hpp"
#include "yqlora/shader/UniformLayout.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace yqlora {

// Место uniform-данных в буфере кадра.
using UniformSlice = BufferSlice;

// Uniform-данные кадра (вид, объекты) подряд в одном буфере, со смещениями,
// выровненными по ограничению устройства.
class YQLORA_API UniformRing : public BufferRing {
public:
    static constexpr std::size_t kInitialCapacity = 64 * 1024;

    explicit UniformRing(rhi::Device& device, std::size_t capacity = kInitialCapacity)
        : BufferRing(device, rhi::BufferUsage::Uniform, device.limits().uniformOffsetAlignment, capacity)
    {
    }

    using BufferRing::push;

    template <typename T>
    UniformSlice push(const T& value)
    {
        return push(std::as_bytes(std::span(&value, 1)));
    }
};

// Данные блока шейдера (набор и привязка - из его раскладки).
inline void setUniforms(rhi::CommandList& commands, const UniformLayout& block, const UniformSlice& slice)
{
    commands.setUniformBuffer(block.slot.set, block.slot.binding, *slice.buffer, slice.offset, slice.size);
}

} // namespace yqlora
