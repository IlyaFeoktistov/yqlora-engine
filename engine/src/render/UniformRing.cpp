#include "yqlora/render/UniformRing.hpp"

#include <algorithm>

namespace yqlora {

namespace {

std::unique_ptr<rhi::Buffer> createUniformBuffer(rhi::Device& device, std::size_t capacity)
{
    return device.createBuffer({.size = capacity, .usage = rhi::BufferUsage::Uniform, .dynamic = true});
}

} // namespace

UniformRing::UniformRing(rhi::Device& device, std::size_t capacity)
    : m_device(device)
    , m_buffer(createUniformBuffer(device, capacity))
    , m_capacity(capacity)
{
}

UniformSlice UniformRing::push(std::span<const std::byte> data)
{
    std::size_t offset = rhi::alignUp(m_cursor, m_device.limits().uniformOffsetAlignment);
    if (offset + data.size() > m_capacity) {
        grow(data.size());
        offset = 0;
    }
    m_buffer->write(offset, data);
    m_cursor = offset + data.size();

    return {m_buffer.get(), offset, data.size()};
}

void UniformRing::reset()
{
    m_retired.clear();
    m_cursor = 0;
}

void UniformRing::grow(std::size_t needed)
{
    m_capacity = std::max(m_capacity * 2, needed);
    m_retired.push_back(std::move(m_buffer));
    m_buffer = createUniformBuffer(m_device, m_capacity);
    m_cursor = 0;
}

} // namespace yqlora
