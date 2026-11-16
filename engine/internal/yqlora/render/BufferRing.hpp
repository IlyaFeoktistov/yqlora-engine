#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/rhi/Device.hpp"

#include <cstddef>
#include <memory>
#include <span>
#include <vector>

namespace yqlora {

// Место данных в буфере кадра.
struct BufferSlice {
    const rhi::Buffer* buffer = nullptr;
    std::size_t offset = 0;
    std::size_t size = 0;
};

// Данные кадра одного назначения (uniform-блоки, вершины отладочных линий)
// подряд в одном буфере, со смещениями, кратными alignment. Не влезло -
// заводится буфер побольше; старые живут до reset(), ведь команды кадра на
// них ещё ссылаются.
class YQLORA_API BufferRing {
public:
    BufferRing(rhi::Device& device, rhi::BufferUsage usage, std::size_t alignment, std::size_t capacity);

    BufferSlice push(std::span<const std::byte> data);

    // Кадр закончен: данные можно писать с начала.
    void reset();

    std::size_t capacity() const
    {
        return m_capacity;
    }

private:
    void grow(std::size_t needed);
    std::unique_ptr<rhi::Buffer> createBuffer() const;

    rhi::Device& m_device;
    rhi::BufferUsage m_usage;
    std::size_t m_alignment;
    std::unique_ptr<rhi::Buffer> m_buffer;
    std::vector<std::unique_ptr<rhi::Buffer>> m_retired;
    std::size_t m_capacity = 0;
    std::size_t m_cursor = 0;
};

} // namespace yqlora
