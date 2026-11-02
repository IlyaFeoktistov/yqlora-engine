#pragma once

#include <cstddef>
#include <span>

namespace yqlora::rhi {

enum class BufferUsage {
    Vertex,
    Index, // индексы всегда std::uint32_t
    Uniform,
};

struct BufferDesc {
    std::size_t size = 0;
    BufferUsage usage = BufferUsage::Vertex;
    // Буфер, который переписывается каждый кадр (uniform-данные). Только в
    // такой можно писать после создания.
    bool dynamic = false;
};

// Буфер в видеопамяти. Владеет создавший; удаление освобождает память, когда
// кончатся кадры, которые могут им пользоваться (см. Device).
class Buffer {
public:
    explicit Buffer(const BufferDesc& desc)
        : m_desc(desc)
    {
    }

    virtual ~Buffer() = default;

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    const BufferDesc& desc() const
    {
        return m_desc;
    }

    // Только у dynamic. Запись видна командам, записанным после неё; что
    // кадры в полёте читают старые данные, устройство решает само.
    virtual void write(std::size_t offset, std::span<const std::byte> data) = 0;

private:
    BufferDesc m_desc;
};

} // namespace yqlora::rhi
