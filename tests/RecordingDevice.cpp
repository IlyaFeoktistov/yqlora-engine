#include "RecordingDevice.hpp"

#include <algorithm>
#include <stdexcept>

namespace yqlora::test {

namespace {

void requireThat(bool condition, const char* message)
{
    if (!condition) {
        throw std::logic_error(message);
    }
}

bool hasUsage(const rhi::TextureDesc& desc, unsigned usage)
{
    return (desc.usage & usage) != 0U;
}

} // namespace

RecordingBuffer::RecordingBuffer(const rhi::BufferDesc& desc, std::span<const std::byte> data)
    : rhi::Buffer(desc)
    , bytes(desc.size)
{
    requireThat(data.size() <= desc.size, "createBuffer: data is larger than the buffer");
    std::ranges::copy(data, bytes.begin());
}

void RecordingBuffer::write(std::size_t offset, std::span<const std::byte> data)
{
    requireThat(desc().dynamic, "write: the buffer is not dynamic");
    requireThat(offset + data.size() <= bytes.size(), "write: past the end of the buffer");
    std::ranges::copy(data, bytes.begin() + static_cast<std::ptrdiff_t>(offset));
}

void RecordingCommandList::beginRenderPass(const rhi::RenderPassDesc& desc)
{
    requireThat(!m_inPass, "beginRenderPass: the previous pass is not ended");
    requireThat(desc.color != nullptr, "beginRenderPass: no colour target");
    requireThat(desc.color->state() == rhi::ResourceState::ColorTarget, "beginRenderPass: colour is not a target");
    const rhi::TextureDesc& color = desc.color->desc();
    requireThat(
        desc.colorFace >= 0 && desc.colorFace < (color.cube ? rhi::kCubeFaces : 1),
        "beginRenderPass: no such face"
    );
    requireThat(desc.colorMip >= 0 && desc.colorMip < rhi::mipLevelCount(color), "beginRenderPass: no such mip");
    requireThat(
        !desc.depth || (desc.colorFace == 0 && desc.colorMip == 0),
        "beginRenderPass: depth with a face or mip"
    );
    if (desc.depth) {
        requireThat(desc.depth->state() == rhi::ResourceState::DepthTarget, "beginRenderPass: depth is not a target");
        requireThat(
            desc.depth->desc().width == desc.color->desc().width
                && desc.depth->desc().height == desc.color->desc().height,
            "beginRenderPass: attachments differ in size"
        );
    }
    m_inPass = true;
    m_target = {desc.color->desc().format, desc.depth ? desc.depth->desc().format : rhi::Format::None, 1};
    m_pipeline = nullptr;
    m_device.recorded.push_back({CommandKind::BeginPass});
}

void RecordingCommandList::endRenderPass()
{
    requireThat(m_inPass, "endRenderPass: no pass is open");
    m_inPass = false;
    m_pipeline = nullptr;
    m_device.recorded.push_back({CommandKind::EndPass});
}

void RecordingCommandList::bindPipeline(const rhi::Pipeline& pipeline)
{
    requireThat(m_inPass, "bindPipeline: outside a render pass");
    rhi::checkPipelineTarget(pipeline.desc(), m_target);
    m_pipeline = &pipeline;
    // Ресурсы задаются заново после каждой смены конвейера.
    m_bound.clear();
    m_hasVertexBuffer = false;
    m_hasIndexBuffer = false;
    m_device.recorded.push_back({CommandKind::BindPipeline, 0, pipeline.desc().shader->desc().name});
}

void RecordingCommandList::setVertexBuffer(const rhi::Buffer& buffer)
{
    requireThat(buffer.desc().usage == rhi::BufferUsage::Vertex, "setVertexBuffer: not a vertex buffer");
    m_hasVertexBuffer = true;
    m_device.recorded.push_back({CommandKind::SetVertexBuffer});
}

void RecordingCommandList::setIndexBuffer(const rhi::Buffer& buffer)
{
    requireThat(buffer.desc().usage == rhi::BufferUsage::Index, "setIndexBuffer: not an index buffer");
    m_hasIndexBuffer = true;
    m_device.recorded.push_back({CommandKind::SetIndexBuffer});
}

void RecordingCommandList::setUniformBuffer(
    std::uint32_t set,
    std::uint32_t slot,
    const rhi::Buffer& buffer,
    std::size_t offset,
    std::size_t size
)
{
    binding(set, slot, rhi::BindingKind::UniformBlock);
    requireThat(buffer.desc().usage == rhi::BufferUsage::Uniform, "setUniformBuffer: not a uniform buffer");
    requireThat(offset % m_device.limits().uniformOffsetAlignment == 0, "setUniformBuffer: unaligned offset");
    requireThat(offset + size <= buffer.desc().size, "setUniformBuffer: past the end of the buffer");
    m_bound.emplace(set, slot);
    m_device.recorded.push_back({CommandKind::SetUniformBuffer, offset});
}

void RecordingCommandList::setTexture(
    std::uint32_t set,
    std::uint32_t slot,
    const rhi::Texture& texture,
    const rhi::Sampler&
)
{
    binding(set, slot, rhi::BindingKind::Texture);
    requireThat(hasUsage(texture.desc(), rhi::kSampled), "setTexture: the texture is not for sampling");
    requireThat(texture.state() == rhi::ResourceState::ShaderRead, "setTexture: the texture is not in ShaderRead");
    m_bound.emplace(set, slot);
    m_device.recorded.push_back({CommandKind::SetTexture});
}

void RecordingCommandList::generateMipmaps(rhi::Texture& texture)
{
    requireThat(!m_inPass, "generateMipmaps: inside a render pass");
    requireThat(texture.desc().mipmaps, "generateMipmaps: the texture has no mipmaps");
    m_device.recorded.push_back({CommandKind::GenerateMipmaps});
}

void RecordingCommandList::draw(std::uint32_t vertexCount, std::uint32_t)
{
    checkDrawable();
    m_device.recorded.push_back({CommandKind::Draw, vertexCount});
}

void RecordingCommandList::drawIndexed(std::uint32_t indexCount, std::uint32_t)
{
    checkDrawable();
    requireThat(m_hasIndexBuffer, "drawIndexed: no index buffer");
    m_device.recorded.push_back({CommandKind::DrawIndexed, indexCount});
}

void RecordingCommandList::recordTransition(rhi::Texture&, rhi::ResourceState, rhi::ResourceState to)
{
    requireThat(!m_inPass, "transition: inside a render pass");
    m_device.recorded.push_back({CommandKind::Transition, static_cast<std::uint64_t>(to)});
}

const rhi::ShaderBinding& RecordingCommandList::binding(
    std::uint32_t set,
    std::uint32_t slot,
    rhi::BindingKind kind
) const
{
    requireThat(m_pipeline != nullptr, "No pipeline is bound");
    const rhi::ShaderBinding* found = m_pipeline->desc().shader->findBinding({set, slot});
    requireThat(found != nullptr, "The shader has no resource at this set and binding");
    requireThat(found->kind == kind, "The shader resource is of another kind");

    return *found;
}

void RecordingCommandList::checkDrawable() const
{
    requireThat(m_inPass && m_pipeline != nullptr, "draw: no pipeline is bound");
    for (const rhi::ShaderBinding& entry : m_pipeline->desc().shader->desc().reflection.bindings) {
        requireThat(m_bound.contains({entry.slot.set, entry.slot.binding}), "draw: a shader resource is not set");
    }
    requireThat(m_pipeline->desc().vertexLayout.attributes.empty() || m_hasVertexBuffer, "draw: no vertex buffer");
}

RecordingDevice::RecordingDevice()
    : m_backBuffer({
          .width = kBackBufferSize,
          .height = kBackBufferSize,
          .format = rhi::kBackBufferColorFormat,
          .usage = rhi::kColorTarget,
      })
    , m_backBufferDepth({
          .width = kBackBufferSize,
          .height = kBackBufferSize,
          .format = rhi::kBackBufferDepthFormat,
          .usage = rhi::kDepthTarget,
      })
    , m_commands(*this)
{
    m_commands.transition(m_backBuffer, rhi::ResourceState::Present);
    recorded.clear();
}

std::unique_ptr<rhi::Buffer> RecordingDevice::createBuffer(const rhi::BufferDesc& desc, std::span<const std::byte> data)
{
    return std::make_unique<RecordingBuffer>(desc, data);
}

std::unique_ptr<rhi::Texture> RecordingDevice::createTexture(
    const rhi::TextureDesc& desc,
    std::span<const std::byte> pixels
)
{
    requireThat(desc.format != rhi::Format::None, "createTexture: no format");
    requireThat(pixels.empty() || hasUsage(desc, rhi::kSampled), "createTexture: pixels for a texture not sampled");

    return std::make_unique<RecordingTexture>(desc);
}

std::unique_ptr<rhi::Sampler> RecordingDevice::createSampler(const rhi::SamplerDesc& desc)
{
    return std::make_unique<rhi::Sampler>(desc);
}

std::unique_ptr<rhi::Shader> RecordingDevice::createShader(rhi::ShaderDesc desc)
{
    return std::make_unique<rhi::Shader>(std::move(desc));
}

std::unique_ptr<rhi::Pipeline> RecordingDevice::createPipeline(rhi::PipelineDesc desc)
{
    requireThat(desc.shader != nullptr, "createPipeline: no shader");
    requireThat(desc.colorFormat != rhi::Format::None, "createPipeline: no colour format");

    return std::make_unique<rhi::Pipeline>(std::move(desc));
}

void RecordingDevice::resizeBackBuffer(int width, int height)
{
    m_backBuffer.resize(width, height);
    m_backBufferDepth.resize(width, height);
}

std::vector<std::byte> RecordingDevice::readPixels(rhi::Texture& texture)
{
    const rhi::TextureDesc& desc = texture.desc();
    requireThat(hasUsage(desc, rhi::kColorTarget), "readPixels: the texture is not a colour target");
    requireThat(!m_commands.inPass(), "readPixels: inside a render pass");
    const std::size_t pixelSize = desc.format == rhi::Format::R8 ? 1 : 4;

    return std::vector<std::byte>(static_cast<std::size_t>(desc.width * desc.height) * pixelSize);
}

void RecordingDevice::present()
{
    requireThat(!m_commands.inPass(), "present: a render pass is open");
    requireThat(m_backBuffer.state() == rhi::ResourceState::Present, "present: the back buffer is not in Present");
    recorded.push_back({CommandKind::Present});
}

std::vector<CommandKind> RecordingDevice::kinds() const
{
    std::vector<CommandKind> result;
    result.reserve(recorded.size());
    for (const Command& command : recorded) {
        result.push_back(command.kind);
    }

    return result;
}

} // namespace yqlora::test
