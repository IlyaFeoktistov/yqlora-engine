#include "yqlora/opengl/GlBuffer.hpp"

#include <glad/gl.h>

#include <stdexcept>

namespace yqlora::opengl {

// Данные идут через GL_COPY_WRITE_BUFFER: привязка индексного буфера к
// GL_ELEMENT_ARRAY_BUFFER поменяла бы привязанный сейчас VAO.
GlBuffer::GlBuffer(const rhi::BufferDesc& desc, std::span<const std::byte> data)
    : rhi::Buffer(desc)
{
    glGenBuffers(1, &m_buffer);
    glBindBuffer(GL_COPY_WRITE_BUFFER, m_buffer);
    glBufferData(
        GL_COPY_WRITE_BUFFER,
        static_cast<GLsizeiptr>(desc.size),
        data.empty() ? nullptr : data.data(),
        desc.dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW
    );
    glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
}

GlBuffer::~GlBuffer()
{
    glDeleteBuffers(1, &m_buffer);
}

void GlBuffer::write(std::size_t offset, std::span<const std::byte> data)
{
    if (!desc().dynamic) {
        throw std::logic_error("Writing into a buffer that is not dynamic");
    }
    glBindBuffer(GL_COPY_WRITE_BUFFER, m_buffer);
    glBufferSubData(
        GL_COPY_WRITE_BUFFER,
        static_cast<GLintptr>(offset),
        static_cast<GLsizeiptr>(data.size()),
        data.data()
    );
    glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
}

} // namespace yqlora::opengl
