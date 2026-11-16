#include "yqlora/render/BufferRing.hpp"

#include <algorithm>

namespace yqlora {

BufferRing::BufferRing(rhi::Device& device, rhi::BufferUsage usage, std::size_t alignment, std::size_t capacity)
    : m_device(device)
    , m_usage(usage)
    , m_alignment(alignment)
    , m_capacity(capacity)
{
    m_buffer = createBuffer();
}

std::unique_ptr<rhi::Buffer> BufferRing::createBuffer() const
{
    return m_device.createBuffer({.size = m_capacity, .usage = m_usage, .dynamic = true});
}

BufferSlice BufferRing::push(std::span<const std::byte> data)
{
    std::size_t offset = rhi::alignUp(m_cursor, m_alignment);
    if (offset + data.size() > m_capacity) {
        grow(data.size());
        offset = 0;
    }
    m_buffer->write(offset, data);
    m_cursor = offset + data.size();

    return {m_buffer.get(), offset, data.size()};
}

void BufferRing::reset()
{
    m_retired.clear();
    m_cursor = 0;
}

void BufferRing::grow(std::size_t needed)
{
    m_capacity = std::max(m_capacity * 2, needed);
    m_retired.push_back(std::move(m_buffer));
    m_buffer = createBuffer();
    m_cursor = 0;
}

} // namespace yqlora
