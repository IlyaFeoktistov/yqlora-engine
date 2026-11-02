#pragma once

#include "yqlora/rhi/Buffer.hpp"

namespace yqlora::opengl {

class GlBuffer final : public rhi::Buffer {
public:
    GlBuffer(const rhi::BufferDesc& desc, std::span<const std::byte> data);
    ~GlBuffer() override;

    void write(std::size_t offset, std::span<const std::byte> data) override;

    unsigned int id() const
    {
        return m_buffer;
    }

private:
    unsigned int m_buffer = 0;
};

} // namespace yqlora::opengl
