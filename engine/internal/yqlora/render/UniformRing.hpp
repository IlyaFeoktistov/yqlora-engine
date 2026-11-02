#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/rhi/Device.hpp"
#include "yqlora/rhi/Pipeline.hpp"
#include "yqlora/shader/UniformLayout.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace yqlora {

// Место uniform-данных в буфере кадра.
struct UniformSlice {
    const rhi::Buffer* buffer = nullptr;
    std::size_t offset = 0;
    std::size_t size = 0;
};

// Uniform-данные кадра (вид, объекты) подряд в одном буфере, со смещениями,
// выровненными по ограничению устройства. Не влезло - заводится буфер
// побольше; старые живут до reset(), ведь команды кадра на них ещё ссылаются.
class YQLORA_API UniformRing {
public:
    static constexpr std::size_t kInitialCapacity = 64 * 1024;

    explicit UniformRing(rhi::Device& device, std::size_t capacity = kInitialCapacity);

    UniformSlice push(std::span<const std::byte> data);

    template <typename T>
    UniformSlice push(const T& value)
    {
        return push(std::as_bytes(std::span(&value, 1)));
    }

    // Кадр закончен: данные можно писать с начала.
    void reset();

    std::size_t capacity() const
    {
        return m_capacity;
    }

private:
    void grow(std::size_t needed);

    rhi::Device& m_device;
    std::unique_ptr<rhi::Buffer> m_buffer;
    std::vector<std::unique_ptr<rhi::Buffer>> m_retired;
    std::size_t m_capacity = 0;
    std::size_t m_cursor = 0;
};

// Данные блока шейдера (набор и привязка - из его раскладки).
inline void setUniforms(rhi::CommandList& commands, const UniformLayout& block, const UniformSlice& slice)
{
    commands.setUniformBuffer(block.slot.set, block.slot.binding, *slice.buffer, slice.offset, slice.size);
}

} // namespace yqlora
