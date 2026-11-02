#include "RecordingDevice.hpp"

#include <doctest/doctest.h>

#include <yqlora/render/FrameData.hpp>
#include <yqlora/render/GpuMesh.hpp>
#include <yqlora/render/MeshData.hpp>
#include <yqlora/render/ShaderData.hpp>
#include <yqlora/render/UniformRing.hpp>

#include <array>
#include <cstring>
#include <stdexcept>
#include <vector>

using namespace yqlora;
using yqlora::test::CommandKind;
using yqlora::test::RecordingBuffer;
using yqlora::test::RecordingDevice;

namespace {

const rhi::ShaderBinding kAlbedo{.name = "albedo", .kind = rhi::BindingKind::Texture, .slot = {kMaterialSet, 1}};

// Отражение блока, как его дал бы шейдер; тестовому устройству хватает места и вида.
rhi::ShaderBinding block(const UniformLayout& layout)
{
    return {
        .name = std::string(layout.name),
        .kind = rhi::BindingKind::UniformBlock,
        .slot = layout.slot,
        .size = static_cast<std::uint32_t>(layout.size),
    };
}

std::unique_ptr<rhi::Shader> meshShader(rhi::Device& device)
{
    return device.createShader({.name = "mesh", .reflection = {.bindings = {block(kViewBlock), block(kObjectBlock)}}});
}

// Проход в задний буфер с его глубиной.
void beginWindowPass(rhi::Device& device)
{
    rhi::CommandList& commands = device.commands();
    commands.transition(device.backBuffer(), rhi::ResourceState::ColorTarget);
    commands.beginRenderPass({.color = &device.backBuffer(), .depth = &device.backBufferDepth()});
}

} // namespace

TEST_CASE("RHI: transition records only a change of state")
{
    RecordingDevice device;
    auto texture = device.createTexture({.usage = rhi::kColorTarget | rhi::kSampled}, {});
    CHECK(texture->state() == rhi::ResourceState::ColorTarget);

    device.commands().transition(*texture, rhi::ResourceState::ColorTarget);
    CHECK(device.recorded.empty());

    device.commands().transition(*texture, rhi::ResourceState::ShaderRead);
    CHECK(texture->state() == rhi::ResourceState::ShaderRead);
    CHECK(device.kinds() == std::vector{CommandKind::Transition});
}

TEST_CASE("RHI: a new texture starts as a target or for reading")
{
    CHECK(rhi::initialState({.usage = rhi::kSampled}) == rhi::ResourceState::ShaderRead);
    CHECK(rhi::initialState({.usage = rhi::kColorTarget | rhi::kSampled}) == rhi::ResourceState::ColorTarget);
    CHECK(rhi::initialState({.usage = rhi::kDepthTarget}) == rhi::ResourceState::DepthTarget);
}

TEST_CASE("RHI: a pipeline draws only into a target of its formats")
{
    RecordingDevice device;
    auto shader = meshShader(device);
    auto mask = device.createTexture({.format = rhi::Format::R8, .usage = rhi::kColorTarget | rhi::kSampled}, {});

    auto windowPipeline = device.createPipeline({.shader = shader.get()});
    device.commands().beginRenderPass({.color = mask.get()});
    CHECK_THROWS_AS(device.commands().bindPipeline(*windowPipeline), std::logic_error);
    device.commands().endRenderPass();

    auto maskPipeline = device.createPipeline({
        .shader = shader.get(),
        .colorFormat = rhi::Format::R8,
        .depthFormat = rhi::Format::None,
    });
    device.commands().beginRenderPass({.color = mask.get()});
    CHECK_NOTHROW(device.commands().bindPipeline(*maskPipeline));
    device.commands().endRenderPass();

    // Та же разница в числе образцов.
    CHECK_THROWS_AS(
        rhi::checkPipelineTarget(
            {.shader = shader.get(), .samples = 4},
            {.color = rhi::Format::Rgba8, .depth = rhi::Format::Depth24Stencil8}
        ),
        std::logic_error
    );
}

TEST_CASE("RHI: the attachments of a pass are in target states")
{
    RecordingDevice device;
    // Между кадрами задний буфер показывается на экране, рисовать в него нельзя.
    CHECK_THROWS_AS(device.commands().beginRenderPass({.color = &device.backBuffer()}), std::logic_error);

    beginWindowPass(device);
    // Смена состояния внутри прохода запрещена.
    CHECK_THROWS_AS(device.commands().transition(device.backBuffer(), rhi::ResourceState::Present), std::logic_error);
    device.commands().endRenderPass();

    // Кадр показывается только из состояния Present.
    CHECK_THROWS_AS(device.present(), std::logic_error);
    device.commands().transition(device.backBuffer(), rhi::ResourceState::Present);
    CHECK_NOTHROW(device.present());
}

TEST_CASE("RHI: a draw needs every resource of the shader set after the pipeline")
{
    RecordingDevice device;
    auto shader = meshShader(device);
    auto pipeline = device.createPipeline({.shader = shader.get()});
    auto uniforms = device.createBuffer({.size = 1024, .usage = rhi::BufferUsage::Uniform, .dynamic = true}, {});
    auto vertices = device.createBuffer({.size = 16, .usage = rhi::BufferUsage::Vertex}, {});

    beginWindowPass(device);
    rhi::CommandList& commands = device.commands();
    commands.bindPipeline(*pipeline);
    commands.setUniformBuffer(kViewSet, 0, *uniforms, 0, sizeof(ViewUniforms));
    CHECK_THROWS_AS(commands.draw(3), std::logic_error);

    commands.setUniformBuffer(kObjectSet, 0, *uniforms, 256, sizeof(ObjectUniforms));
    CHECK_NOTHROW(commands.draw(3));

    // После смены конвейера ресурсы задаются заново.
    commands.bindPipeline(*pipeline);
    CHECK_THROWS_AS(commands.draw(3), std::logic_error);
    commands.endRenderPass();

    // Смещение кратно ограничению устройства, ресурс есть у шейдера.
    beginWindowPass(device);
    commands.bindPipeline(*pipeline);
    CHECK_THROWS_AS(commands.setUniformBuffer(kViewSet, 0, *uniforms, 16, sizeof(ViewUniforms)), std::logic_error);
    CHECK_THROWS_AS(commands.setUniformBuffer(kFrameSet, 0, *uniforms, 0, 16), std::logic_error);
    CHECK_THROWS_AS(commands.setUniformBuffer(kViewSet, 0, *vertices, 0, 16), std::logic_error);
    commands.endRenderPass();
}

TEST_CASE("RHI: a texture is read only in ShaderRead")
{
    RecordingDevice device;
    auto shader = device.createShader({.name = "textured", .reflection = {.bindings = {kAlbedo}}});
    auto pipeline = device.createPipeline({.shader = shader.get()});
    auto sampler = device.createSampler({});
    auto target = device.createTexture({.usage = rhi::kColorTarget | rhi::kSampled}, {});

    beginWindowPass(device);
    rhi::CommandList& commands = device.commands();
    commands.bindPipeline(*pipeline);
    CHECK_THROWS_AS(commands.setTexture(kAlbedo.slot.set, kAlbedo.slot.binding, *target, *sampler), std::logic_error);
    commands.endRenderPass();

    commands.transition(*target, rhi::ResourceState::ShaderRead);
    beginWindowPass(device);
    commands.bindPipeline(*pipeline);
    commands.setTexture(kAlbedo.slot.set, kAlbedo.slot.binding, *target, *sampler);
    CHECK_NOTHROW(commands.draw(3));
    commands.endRenderPass();
}

TEST_CASE("RHI: only a dynamic buffer is written after creation")
{
    RecordingDevice device;
    const std::array<std::byte, 4> data{};
    auto fixed = device.createBuffer({.size = data.size(), .usage = rhi::BufferUsage::Vertex}, data);
    CHECK_THROWS_AS(fixed->write(0, data), std::logic_error);

    auto dynamic = device.createBuffer({.size = data.size(), .usage = rhi::BufferUsage::Uniform, .dynamic = true}, {});
    CHECK_NOTHROW(dynamic->write(0, data));
    CHECK_THROWS_AS(dynamic->write(1, data), std::logic_error);
}

TEST_CASE("UniformRing: slices are aligned and survive growth until reset")
{
    RecordingDevice device;
    const std::size_t alignment = device.limits().uniformOffsetAlignment;
    UniformRing ring(device, 2 * alignment);

    const UniformSlice first = ring.push(ViewUniforms{.cameraPosition = Vec3(1.0f, 2.0f, 3.0f)});
    const UniformSlice second = ring.push(FrameUniforms{});
    CHECK(first.offset == 0);
    CHECK(first.size == sizeof(ViewUniforms));
    CHECK(second.offset == alignment);
    CHECK(second.buffer == first.buffer);

    // Не влезло: новый буфер побольше, старый со своими данными ещё жив.
    const UniformSlice third = ring.push(ObjectUniforms{});
    CHECK(third.buffer != first.buffer);
    CHECK(third.offset == 0);
    CHECK(ring.capacity() == 4 * alignment);
    ViewUniforms stored;
    std::memcpy(&stored, static_cast<const RecordingBuffer*>(first.buffer)->bytes.data(), sizeof(stored));
    CHECK(stored.cameraPosition == Vec3(1.0f, 2.0f, 3.0f));

    ring.reset();
    CHECK(ring.push(FrameUniforms{}).offset == 0);
}

TEST_CASE("GpuMesh: a mesh goes to buffers and draws all its indices")
{
    RecordingDevice device;
    MeshData mesh;
    mesh.vertices = {{Vec3(0.0f), Vec3(0.0f, 1.0f, 0.0f)}, {Vec3(1.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f)}};
    mesh.indices = {0, 1, 0};
    const auto gpu = uploadMesh(device, mesh);
    CHECK(gpu->vertices->desc().size == 2 * sizeof(Vertex));
    CHECK(gpu->indices->desc().usage == rhi::BufferUsage::Index);
    CHECK(gpu->indexCount == 3);

    const rhi::VertexLayout layout = meshVertexLayout();
    CHECK(layout.stride == sizeof(Vertex));
    CHECK(layout.attributes.size() == 3);

    auto shader = device.createShader({.name = "plain"});
    auto pipeline = device.createPipeline({.shader = shader.get(), .vertexLayout = layout});
    beginWindowPass(device);
    device.commands().bindPipeline(*pipeline);
    device.recorded.clear();
    drawMesh(device.commands(), *gpu);
    device.commands().endRenderPass();

    CHECK(
        device.kinds()
        == std::vector{
            CommandKind::SetVertexBuffer,
            CommandKind::SetIndexBuffer,
            CommandKind::DrawIndexed,
            CommandKind::EndPass
        }
    );
    CHECK(device.recorded[2].value == 3);
}

TEST_CASE("GpuMeshCache: a new revision replaces the copy")
{
    RecordingDevice device;
    GpuMeshCache cache(device);
    MeshData mesh;
    mesh.vertices = {{Vec3(0.0f), Vec3(0.0f, 1.0f, 0.0f)}};
    mesh.indices = {0, 0, 0};

    DrawItem item{.mesh = &mesh, .meshId = 7, .meshRevision = 1};
    const GpuMesh* first = &cache.get(item);
    CHECK(&cache.get(item) == first);

    mesh.indices = {0, 0, 0, 0, 0, 0};
    item.meshRevision = 2;
    CHECK(cache.get(item).indexCount == 6);
}
