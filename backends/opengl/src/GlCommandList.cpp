#include "yqlora/opengl/GlCommandList.hpp"

#include "yqlora/opengl/GlBuffer.hpp"
#include "yqlora/opengl/GlDevice.hpp"
#include "yqlora/opengl/GlShader.hpp"
#include "yqlora/opengl/GlTexture.hpp"

#include <glad/gl.h>

#include <stdexcept>
#include <string>

namespace yqlora::opengl {

namespace {

GLint componentCount(rhi::VertexFormat format)
{
    switch (format) {
    case rhi::VertexFormat::Float2: return 2;
    case rhi::VertexFormat::Float3: return 3;
    }

    return 0;
}

void requireThat(bool condition, const char* message)
{
    if (!condition) {
        throw std::logic_error(message);
    }
}

} // namespace

GlCommandList::GlCommandList(GlDevice& device)
    : m_device(device)
{
    // Core profile не рисует без привязанного VAO, даже если атрибутов нет.
    glGenVertexArrays(1, &m_vertexArray);
}

GlCommandList::~GlCommandList()
{
    glDeleteVertexArrays(1, &m_vertexArray);
}

void GlCommandList::beginRenderPass(const rhi::RenderPassDesc& desc)
{
    requireThat(!m_inPass, "beginRenderPass: the previous pass is not ended");
    requireThat(desc.color != nullptr, "beginRenderPass: no colour target");
    const auto& color = static_cast<const GlTexture&>(*desc.color);
    const auto* depth = static_cast<const GlTexture*>(desc.depth);

    GLuint framebuffer = 0;
    if (color.isBackBuffer()) {
        requireThat(!depth || depth->isBackBuffer(), "beginRenderPass: the back buffer has its own depth");
    } else {
        framebuffer = m_device.framebuffer(color, depth);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glBindVertexArray(m_vertexArray);
    glViewport(0, 0, color.desc().width, color.desc().height);
    // Интерфейс оставляет включённым обрезание по прямоугольнику.
    glDisable(GL_SCISSOR_TEST);

    GLbitfield clear = 0;
    if (desc.colorLoad == rhi::LoadOp::Clear) {
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glClearColor(desc.clearColor.r, desc.clearColor.g, desc.clearColor.b, desc.clearColor.a);
        clear |= GL_COLOR_BUFFER_BIT;
    }
    if (depth && desc.depthLoad == rhi::LoadOp::Clear) {
        // Запись глубины выключенная прошлым конвейером не дала бы её стереть.
        glDepthMask(GL_TRUE);
        glClearDepth(desc.clearDepth);
        clear |= GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
    }
    if (clear != 0) {
        glClear(clear);
    }

    m_inPass = true;
    m_target = {color.desc().format, depth ? depth->desc().format : rhi::Format::None, 1};
    m_pipeline = nullptr;
    m_vertexBuffer = nullptr;
}

void GlCommandList::endRenderPass()
{
    requireThat(m_inPass, "endRenderPass: no pass is open");
    m_inPass = false;
    m_pipeline = nullptr;
    // Интерфейс рисует в окно и ждёт привязанным framebuffer 0.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GlCommandList::bindPipeline(const rhi::Pipeline& pipeline)
{
    requireThat(m_inPass, "bindPipeline: outside a render pass");
    const rhi::PipelineDesc& desc = pipeline.desc();
    rhi::checkPipelineTarget(desc, m_target);

    glUseProgram(static_cast<const GlShader&>(*desc.shader).program());
    if (desc.depthTest) {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
    } else {
        glDisable(GL_DEPTH_TEST);
    }
    glDepthMask(desc.depthWrite ? GL_TRUE : GL_FALSE);
    if (desc.cull == rhi::CullMode::Back) {
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);
    } else {
        glDisable(GL_CULL_FACE);
    }
    if (desc.blend == rhi::BlendMode::Alpha) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else {
        glDisable(GL_BLEND);
    }

    m_pipeline = &pipeline;
    m_layoutDirty = true;
}

void GlCommandList::setVertexBuffer(const rhi::Buffer& buffer)
{
    m_vertexBuffer = &static_cast<const GlBuffer&>(buffer);
    m_layoutDirty = true;
}

void GlCommandList::setIndexBuffer(const rhi::Buffer& buffer)
{
    // Привязка индексов - часть состояния VAO, он привязан с начала прохода.
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, static_cast<const GlBuffer&>(buffer).id());
}

void GlCommandList::setUniformBuffer(
    std::uint32_t set,
    std::uint32_t binding,
    const rhi::Buffer& buffer,
    std::size_t offset,
    std::size_t size
)
{
    requireThat(offset % m_device.limits().uniformOffsetAlignment == 0, "setUniformBuffer: unaligned offset");
    glBindBufferRange(
        GL_UNIFORM_BUFFER,
        slot(set, binding),
        static_cast<const GlBuffer&>(buffer).id(),
        static_cast<GLintptr>(offset),
        static_cast<GLsizeiptr>(size)
    );
}

void GlCommandList::setTexture(
    std::uint32_t set,
    std::uint32_t binding,
    const rhi::Texture& texture,
    const rhi::Sampler& sampler
)
{
    const unsigned int unit = slot(set, binding);
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, static_cast<const GlTexture&>(texture).id());
    glBindSampler(unit, static_cast<const GlSampler&>(sampler).id());
}

void GlCommandList::draw(std::uint32_t vertexCount, std::uint32_t firstVertex)
{
    applyVertexLayout();
    glDrawArrays(GL_TRIANGLES, static_cast<GLint>(firstVertex), static_cast<GLsizei>(vertexCount));
}

void GlCommandList::drawIndexed(std::uint32_t indexCount, std::uint32_t firstIndex)
{
    applyVertexLayout();
    const auto offset = static_cast<std::uintptr_t>(firstIndex) * sizeof(std::uint32_t);
    glDrawElements(
        GL_TRIANGLES,
        static_cast<GLsizei>(indexCount),
        GL_UNSIGNED_INT,
        reinterpret_cast<const void*>(offset)
    );
}

const GlShader& GlCommandList::shader() const
{
    requireThat(m_pipeline != nullptr, "No pipeline is bound");

    return static_cast<const GlShader&>(*m_pipeline->desc().shader);
}

unsigned int GlCommandList::slot(std::uint32_t set, std::uint32_t binding) const
{
    const std::optional<unsigned int> found = shader().slot(set, binding);
    if (!found) {
        throw std::logic_error(
            "Shader '" + shader().desc().name + "' has no resource at set " + std::to_string(set) + ", binding "
            + std::to_string(binding)
        );
    }

    return *found;
}

void GlCommandList::applyVertexLayout()
{
    requireThat(m_inPass && m_pipeline != nullptr, "draw: no pipeline is bound");
    if (!m_layoutDirty) {
        return;
    }
    const rhi::VertexLayout& layout = m_pipeline->desc().vertexLayout;
    requireThat(layout.attributes.empty() || m_vertexBuffer != nullptr, "draw: no vertex buffer");

    unsigned int enabled = 0;
    if (!layout.attributes.empty()) {
        glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer->id());
        for (const rhi::VertexAttribute& attribute : layout.attributes) {
            glEnableVertexAttribArray(attribute.location);
            glVertexAttribPointer(
                attribute.location,
                componentCount(attribute.format),
                GL_FLOAT,
                GL_FALSE,
                static_cast<GLsizei>(layout.stride),
                reinterpret_cast<const void*>(attribute.offset)
            );
            enabled |= 1U << attribute.location;
        }
    }
    // Входы прошлой раскладки, которых в этой нет.
    for (unsigned int location = 0; (m_enabledAttributes >> location) != 0U; ++location) {
        if ((m_enabledAttributes & ~enabled & (1U << location)) != 0U) {
            glDisableVertexAttribArray(location);
        }
    }
    m_enabledAttributes = enabled;
    m_layoutDirty = false;
}

} // namespace yqlora::opengl
