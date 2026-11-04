#include "RecordingDevice.hpp"
#include "TestFrames.hpp"

#include <yqlora/editor/render/GridPass.hpp>
#include <yqlora/editor/render/OutlinePass.hpp>

#include <doctest/doctest.h>

#include <yqlora/material/MaterialOverrides.hpp>
#include <yqlora/render/EngineUniforms.hpp>
#include <yqlora/render/EnvironmentLighting.hpp>
#include <yqlora/render/FrameData.hpp>
#include <yqlora/render/FrameMaterials.hpp>
#include <yqlora/render/GpuMesh.hpp>
#include <yqlora/render/MeshData.hpp>
#include <yqlora/render/Prepare.hpp>
#include <yqlora/render/PrimitiveMeshes.hpp>
#include <yqlora/render/RenderGraph.hpp>
#include <yqlora/render/Renderer.hpp>
#include <yqlora/render/UniformRing.hpp>
#include <yqlora/resource/BuiltinResources.hpp>
#include <yqlora/resource/ResourceCache.hpp>

#include <algorithm>
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

// Камера в (0, 0, 5) смотрит на начало координат по -Z.
const CameraData kTestCamera{
    .view = glm::lookAt(Vec3(0.0f, 0.0f, 5.0f), Vec3(0.0f), Vec3(0.0f, 1.0f, 0.0f)),
    .projection = math::perspective(glm::radians(60.0f), 1.0f, 0.1f, 100.0f),
    .position = Vec3(0.0f, 0.0f, 5.0f),
};

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
    const UniformSlice second = ring.push(ObjectUniforms{});
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
    CHECK(ring.push(ObjectUniforms{}).offset == 0);
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
    CHECK(layout.attributes.size() == 4);

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

TEST_CASE("Renderer: a frame with the editor passes goes through the RHI contracts")
{
    // Рендерер не знает API: на тестовом устройстве он проходит все его проверки
    // (состояния целей, форматы конвейеров, ресурсы шейдеров перед draw).
    auto owned = std::make_unique<RecordingDevice>();
    RecordingDevice& device = *owned;
    Renderer renderer(GraphicsApi::OpenGL, std::move(owned), nullptr);
    editor::GridPass grid(renderer.context());
    editor::OutlinePass outline(renderer.context());
    outline.setSelection({7}, Vec4(1.0f));

    const MeshData cube = makeCube();
    test::TestMaterials materials;
    FrameData frame;
    frame.views.push_back({.camera = kTestCamera, .passes = {&grid, &outline}});
    frame.drawItems.push_back(
        {.mesh = &cube,
         .meshId = 1,
         .indexCount = static_cast<std::uint32_t>(cube.indices.size()),
         .bounds = cube.bounds,
         .material = materials.add(frame),
         .node = 7}
    );

    auto target = renderer.createRenderTarget(32, 32);
    CHECK_NOTHROW(renderer.render(frame, *target));
    CHECK(target->color().state() == rhi::ResourceState::ShaderRead);
    CHECK_NOTHROW(renderer.render(frame));
    CHECK_NOTHROW(renderer.present());

    const std::vector<CommandKind> kinds = device.kinds();
    // Меш и маска контура, в цель и в окно.
    CHECK(std::ranges::count(kinds, CommandKind::DrawIndexed) == 4);
    // Проход вывода, сетка и полноэкранный контур, в цель и в окно; и один
    // раз за всё время - таблица BRDF света неба.
    CHECK(std::ranges::count(kinds, CommandKind::Draw) == 7);
    CHECK(std::ranges::count(kinds, CommandKind::Present) == 1);
    // Без связки с интерфейсом текстуры для него нет.
    CHECK_THROWS_AS(target->uiTexture(), std::logic_error);
}

TEST_CASE("Renderer: what a view does not see is not drawn")
{
    auto owned = std::make_unique<RecordingDevice>();
    RecordingDevice& device = *owned;
    Renderer renderer(GraphicsApi::OpenGL, std::move(owned), nullptr);
    editor::OutlinePass outline(renderer.context());
    outline.setSelection({7}, Vec4(1.0f));

    // Выбранный куб за спиной камеры: ни меша, ни маски, ни контура.
    const MeshData cube = makeCube();
    const Mat4 behind = glm::translate(Mat4(1.0f), Vec3(0.0f, 0.0f, 20.0f));
    test::TestMaterials materials;
    FrameData frame;
    frame.views.push_back({.camera = kTestCamera, .passes = {&outline}});
    frame.drawItems.push_back({
        .mesh = &cube,
        .meshId = 1,
        .indexCount = static_cast<std::uint32_t>(cube.indices.size()),
        .world = behind,
        .bounds = transformBounds(cube.bounds, behind),
        .material = materials.add(frame),
        .node = 7,
    });
    // Первый кадр ещё строит таблицы света неба: считаем второй.
    renderer.render(frame);
    device.recorded.clear();
    renderer.render(frame);

    const std::vector<CommandKind> kinds = device.kinds();
    CHECK(std::ranges::count(kinds, CommandKind::DrawIndexed) == 0);
    // Остаются только сцена вида (пустая) и проход вывода.
    CHECK(std::ranges::count(kinds, CommandKind::Draw) == 1);
    CHECK(std::ranges::count(kinds, CommandKind::BeginPass) == 2);
}

TEST_CASE("EnvironmentLighting: the sky is captured again only when it changes")
{
    auto owned = std::make_unique<RecordingDevice>();
    RecordingDevice& device = *owned;
    Renderer renderer(GraphicsApi::OpenGL, std::move(owned), nullptr);
    EnvironmentLighting lighting(renderer.context());
    TransientTextures transients(device);

    ResourceCache cache;
    FrameData frame;
    frame.views.push_back({.camera = kTestCamera});
    FrameMaterials materials(cache, frame);
    frame.environment.ambientSky = materials.sky(builtin::kGradientSkyMaterial);
    const auto build = [&] {
        RenderGraph graph(transients);
        lighting.addTo(graph, frame, {}, {});
        graph.execute(device.commands());
        transients.endFrame();
    };

    build();
    CHECK(lighting.captures() == 1);
    build();
    CHECK(lighting.captures() == 1);

    // Другие цвета неба - новый захват.
    MaterialOverrides colors;
    colors.setColor("skyColor", Vec3(1.0f, 0.0f, 0.0f));
    frame.environment.ambientOverrides = materials.addOverrides(*frame.environment.ambientSky, colors);
    build();
    CHECK(lighting.captures() == 2);
    // Камера поднялась меньше чем на шаг - небо то же.
    frame.views.front().camera.position.y += EnvironmentLighting::kHeightStep * 0.5f;
    build();
    CHECK(lighting.captures() == 2);
    frame.views.front().camera.position.y += EnvironmentLighting::kHeightStep;
    build();
    CHECK(lighting.captures() == 3);

    // Без света неба таблицы стираются, а не захватываются.
    frame.environment.ambientSky.reset();
    build();
    CHECK(lighting.captures() == 3);
}

TEST_CASE("EnvironmentLighting: frames with different skies in turn keep their own tables")
{
    auto owned = std::make_unique<RecordingDevice>();
    RecordingDevice& device = *owned;
    Renderer renderer(GraphicsApi::OpenGL, std::move(owned), nullptr);
    EnvironmentLighting lighting(renderer.context());
    TransientTextures transients(device);

    ResourceCache cache;
    const auto frameAt = [&](float height) {
        FrameData frame;
        frame.views.push_back({.camera = kTestCamera});
        frame.views.front().camera.position.y = height;
        FrameMaterials materials(cache, frame);
        frame.environment.ambientSky = materials.sky(builtin::kGradientSkyMaterial);

        return frame;
    };
    const auto build = [&](const FrameData& frame) {
        RenderGraph graph(transients);
        const EnvironmentLighting::Textures textures = lighting.addTo(graph, frame, {}, {});
        graph.execute(device.commands());
        transients.endFrame();

        return textures;
    };

    // Сцена и превью по очереди: каждое небо захватывается один раз.
    const FrameData scene = frameAt(0.0f);
    const FrameData preview = frameAt(EnvironmentLighting::kHeightStep * 3.0f);
    for (int i = 0; i < 3; ++i) {
        build(scene);
        build(preview);
    }
    CHECK(lighting.captures() == 2);

    // Небес больше, чем наборов: уходит то, что дольше всех не нужно.
    for (std::size_t i = 0; i < EnvironmentLighting::kSets; ++i) {
        build(frameAt(EnvironmentLighting::kHeightStep * static_cast<float>(10 + i)));
    }
    CHECK(lighting.captures() == 2 + EnvironmentLighting::kSets);
    build(scene);
    CHECK(lighting.captures() == 3 + EnvironmentLighting::kSets);
}
