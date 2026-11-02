// Небольшие проверки на настоящем OpenGL: скрытое окно ради контекста. Где
// контекст не создать (сервер сборки без экрана), тесты пропускаются с
// сообщением.

#include <doctest/doctest.h>

#include <yqlora/editor/render/GridPass.hpp>
#include <yqlora/editor/render/OutlinePass.hpp>
#include <yqlora/platform/Platform.hpp>
#include <yqlora/platform/Window.hpp>
#include <yqlora/render/EngineUniforms.hpp>
#include <yqlora/render/GraphicsBackend.hpp>
#include <yqlora/render/MeshData.hpp>
#include <yqlora/render/OutputPass.hpp>
#include <yqlora/render/Prepare.hpp>
#include <yqlora/render/PrimitiveMeshes.hpp>
#include <yqlora/render/RenderGraph.hpp>
#include <yqlora/render/Renderer.hpp>
#include <yqlora/render/ViewPass.hpp>
#include <yqlora/shader/EngineShaders.hpp>
#include <yqlora/shader/ShaderBuild.hpp>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <optional>
#include <string>

using namespace yqlora;

namespace {

constexpr int kSize = 64;

// Окно и рендерер над ним. Рендерер (с устройством) уходит раньше окна.
struct GlFixture {
    std::unique_ptr<Window> window;
    std::unique_ptr<Renderer> renderer;

    ~GlFixture()
    {
        renderer.reset();
        window.reset();
        platform::shutdown();
    }
};

std::unique_ptr<GlFixture> openGl()
{
    if (!isBuilt(GraphicsApi::OpenGL) || !platform::initialize()) {
        MESSAGE("skipped: no video");

        return nullptr;
    }
    auto fixture = std::make_unique<GlFixture>();
    try {
        prepareWindowForApi(GraphicsApi::OpenGL);
        fixture->window = std::make_unique<Window>(WindowDesc{
            .title = "yqlora tests",
            .width = kSize,
            .height = kSize,
            .resizable = false,
            .visible = false,
            .api = GraphicsApi::OpenGL,
        });
        fixture->renderer = std::make_unique<Renderer>(
            GraphicsApi::OpenGL,
            createDevice(GraphicsApi::OpenGL, *fixture->window),
            nullptr
        );
    } catch (const std::exception& error) {
        MESSAGE("skipped: no OpenGL context: " << error.what());

        return nullptr;
    }

    return fixture;
}

using Pixel = std::array<std::uint8_t, 4>;

Pixel pixelAt(const std::vector<std::byte>& pixels, int x, int y, int width)
{
    const std::size_t at = (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x))
                           * 4;

    return {
        std::to_integer<std::uint8_t>(pixels[at]),
        std::to_integer<std::uint8_t>(pixels[at + 1]),
        std::to_integer<std::uint8_t>(pixels[at + 2]),
        std::to_integer<std::uint8_t>(pixels[at + 3]),
    };
}

std::uint8_t toByte(float value)
{
    constexpr float kMax = 255.0f;

    return static_cast<std::uint8_t>(value * kMax + 0.5f);
}

CameraData cameraAtZ(float z)
{
    return {
        .view = glm::lookAt(Vec3(0.0f, 0.0f, z), Vec3(0.0f), Vec3(0.0f, 1.0f, 0.0f)),
        .projection = math::perspective(glm::radians(60.0f), 1.0f, 0.1f, 100.0f),
        .position = Vec3(0.0f, 0.0f, z),
    };
}

// Насколько канал цвета поверхности ярче остальных. Блик неметалла белый и
// поднимает все каналы одинаково, поэтому сравниваем каналы, а не ждём нулей.
constexpr int kDominantMargin = 100;

// Белый направленный свет вдоль dir.
LightData directional(const Vec3& dir)
{
    return {.kind = LightKind::Directional, .direction = glm::normalize(dir), .color = Vec3(1.0f)};
}

// Свет со стороны камеры (она на +Z и смотрит на -Z): освещает то, что камера видит.
const LightData kFrontLight = directional(Vec3(0.0f, 0.0f, -1.0f));

// Куб в начале координат перед камерой cameraAtZ(3) в кадре с этим светом;
// цвет пикселя посередине.
Pixel litCubeCenter(Renderer& renderer, const std::vector<LightData>& lights)
{
    const MeshData cube = makeCube();
    FrameData frame;
    frame.views.push_back({.camera = cameraAtZ(3.0f)});
    frame.drawItems.push_back({.mesh = &cube, .meshId = 1, .bounds = cube.bounds, .color = Vec4(1.0f)});
    frame.lights = lights;
    auto target = renderer.createRenderTarget(kSize, kSize);
    renderer.render(frame, *target);

    return pixelAt(renderer.device().readPixels(target->color()), kSize / 2, kSize / 2, kSize);
}

} // namespace

TEST_CASE("OpenGL: an uploaded texture is sampled and read back the right way up")
{
    const auto gl = openGl();
    if (!gl) {
        return;
    }
    rhi::Device& device = gl->renderer->device();

    // 2 x 2: сверху красный и зелёный, снизу синий и белый.
    // clang-format off
    const std::array<std::uint8_t, 16> image{
        255, 0, 0, 255,    0, 255, 0, 255,
        0, 0, 255, 255,    255, 255, 255, 255,
    };
    // clang-format on
    auto texture = device.createTexture({.width = 2, .height = 2}, std::as_bytes(std::span(image)));
    auto sampler = device.createSampler({.filter = rhi::Filter::Nearest, .wrap = rhi::Wrap::Clamp, .mipmaps = false});
    auto target = device.createTexture({.width = 2, .height = 2, .usage = rhi::kColorTarget | rhi::kSampled});

    // Верх экрана - v = 0, первая строка картинки.
    const std::string fragment = "#version 450\n"
                                 "layout(location = 0) in vec2 vUv;\n"
                                 "layout(set = 2, binding = 0) uniform sampler2D image;\n"
                                 "layout(location = 0) out vec4 fragColor;\n"
                                 "void main() { fragColor = texture(image, vec2(vUv.x, 1.0 - vUv.y)); }\n";
    ShaderCompiler compiler([&](const std::string& path) -> std::optional<std::string> {
        return path == "test/image.frag" ? std::optional(fragment) : std::nullopt;
    });
    auto shader = device.createShader(buildBuiltinShader(
        compiler,
        {.name = "image", .vertexPath = engineShaderPath("fullscreen.vert"), .fragmentPath = "test/image.frag"}
    ));
    auto pipeline = device.createPipeline({
        .shader = shader.get(),
        .depthTest = false,
        .depthWrite = false,
        .depthFormat = rhi::Format::None,
    });

    rhi::CommandList& commands = device.commands();
    commands.beginRenderPass({.color = target.get()});
    commands.bindPipeline(*pipeline);
    commands.setTexture(2, 0, *texture, *sampler);
    commands.draw(3);
    commands.endRenderPass();

    const std::vector<std::byte> pixels = device.readPixels(*target);
    CHECK(pixelAt(pixels, 0, 0, 2) == Pixel{255, 0, 0, 255});
    CHECK(pixelAt(pixels, 1, 0, 2) == Pixel{0, 255, 0, 255});
    CHECK(pixelAt(pixels, 0, 1, 2) == Pixel{0, 0, 255, 255});
    CHECK(pixelAt(pixels, 1, 1, 2) == Pixel{255, 255, 255, 255});
}

TEST_CASE("OpenGL: a frame with a mesh and the editor passes is drawn into a target")
{
    const auto gl = openGl();
    if (!gl) {
        return;
    }
    Renderer& renderer = *gl->renderer;
    editor::GridPass grid(renderer.context());
    editor::OutlinePass outline(renderer.context());
    const Vec4 outlineColor(1.0f, 0.6f, 0.1f, 1.0f);
    outline.setSelection({7}, outlineColor);

    const Vec4 background(0.0f, 0.0f, 1.0f, 1.0f);
    const MeshData cube = makeCube();
    FrameData frame;
    frame.views.push_back({.camera = cameraAtZ(3.0f), .background = background, .passes = {&grid, &outline}});
    frame.drawItems.push_back({
        .mesh = &cube,
        .meshId = 1,
        .bounds = cube.bounds,
        .color = Vec4(1.0f, 0.0f, 0.0f, 1.0f),
        .node = 7,
    });
    frame.lights.push_back(kFrontLight);

    auto target = renderer.createRenderTarget(kSize, kSize);
    renderer.render(frame, *target);
    const std::vector<std::byte> pixels = renderer.device().readPixels(target->color());

    // Посередине - освещённый красный куб, в углу - фон, вокруг куба - контур.
    const Pixel center = pixelAt(pixels, kSize / 2, kSize / 2, kSize);
    CHECK(int(center[0]) > int(center[1]) + kDominantMargin);
    CHECK(center[1] == center[2]);
    CHECK(pixelAt(pixels, 0, 0, kSize) == Pixel{0, 0, 255, 255});

    // Цвет контура с точностью до округления драйвера (0.1 * 255 = 25.5).
    const Pixel outlinePixel{toByte(outlineColor.r), toByte(outlineColor.g), toByte(outlineColor.b), 255};
    const auto close = [&](const Pixel& pixel) {
        for (std::size_t channel = 0; channel < pixel.size(); ++channel) {
            if (std::abs(int(pixel[channel]) - int(outlinePixel[channel])) > 1) {
                return false;
            }
        }

        return true;
    };
    bool outlined = false;
    for (int y = 0; y < kSize && !outlined; ++y) {
        for (int x = 0; x < kSize && !outlined; ++x) {
            outlined = close(pixelAt(pixels, x, y, kSize));
        }
    }
    CHECK(outlined);
}

TEST_CASE("OpenGL: the nearer object wins the depth test with clip depth 0..1")
{
    const auto gl = openGl();
    if (!gl) {
        return;
    }
    Renderer& renderer = *gl->renderer;
    const MeshData cube = makeCube();
    // Зелёный ближе к камере, красный дальше; в кадре красный идёт позже.
    const Mat4 near = glm::translate(Mat4(1.0f), Vec3(0.0f, 0.0f, 1.0f));
    const Mat4 far = glm::translate(Mat4(1.0f), Vec3(0.0f, 0.0f, -1.0f));
    FrameData frame;
    frame.views.push_back({.camera = cameraAtZ(4.0f)});
    frame.lights.push_back(kFrontLight);
    frame.drawItems.push_back({
        .mesh = &cube,
        .meshId = 1,
        .world = near,
        .bounds = transformBounds(cube.bounds, near),
        .color = Vec4(0.0f, 1.0f, 0.0f, 1.0f),
    });
    frame.drawItems.push_back({
        .mesh = &cube,
        .meshId = 1,
        .world = far,
        .bounds = transformBounds(cube.bounds, far),
        .color = Vec4(1.0f, 0.0f, 0.0f, 1.0f),
    });

    auto target = renderer.createRenderTarget(kSize, kSize);
    renderer.render(frame, *target);
    const Pixel center = pixelAt(renderer.device().readPixels(target->color()), kSize / 2, kSize / 2, kSize);
    CHECK(int(center[1]) > int(center[0]) + kDominantMargin);
}

TEST_CASE("OpenGL: the side of a cube facing a directional light is brighter than the side away from it")
{
    const auto gl = openGl();
    if (!gl) {
        return;
    }
    // Без окружающего света сторона спиной к свету чёрная.
    const Pixel facing = litCubeCenter(*gl->renderer, {kFrontLight});
    const Pixel away = litCubeCenter(*gl->renderer, {directional(Vec3(0.0f, 0.0f, 1.0f))});
    CHECK(facing[0] > 200);
    CHECK(away[0] == 0);
}

TEST_CASE("OpenGL: a point light out of its range adds nothing")
{
    const auto gl = openGl();
    if (!gl) {
        return;
    }
    // Свет на оси камеры в 2,5 м от передней грани куба.
    LightData point{.kind = LightKind::Point, .position = Vec3(0.0f, 0.0f, 3.0f), .color = Vec3(10.0f)};
    point.range = 2.0f;
    CHECK(litCubeCenter(*gl->renderer, {point})[0] == 0);
    point.range = 10.0f;
    CHECK(litCubeCenter(*gl->renderer, {point})[0] > 50);
}

TEST_CASE("OpenGL: half transparent white over black blends in linear RGB")
{
    const auto gl = openGl();
    if (!gl) {
        return;
    }
    Renderer& renderer = *gl->renderer;
    rhi::Device& device = renderer.device();
    const std::string fragment = "#version 450\n"
                                 "layout(location = 0) out vec4 fragColor;\n"
                                 "void main() { fragColor = vec4(1.0, 1.0, 1.0, 0.5); }\n";
    ShaderCompiler compiler([&](const std::string& path) -> std::optional<std::string> {
        return path == "test/half.frag" ? std::optional(fragment) : std::nullopt;
    });
    auto shader = device.createShader(buildBuiltinShader(
        compiler,
        {.name = "half", .vertexPath = engineShaderPath("fullscreen.vert"), .fragmentPath = "test/half.frag"}
    ));
    auto pipeline = device.createPipeline({
        .shader = shader.get(),
        .depthTest = false,
        .depthWrite = false,
        .cull = rhi::CullMode::None,
        .blend = rhi::BlendMode::Alpha,
        .colorFormat = kHdrColorFormat,
        .depthFormat = rhi::Format::None,
    });
    auto final = device.createTexture({.width = kSize, .height = kSize, .usage = rhi::kColorTarget | rhi::kSampled});
    const OutputPass output(renderer.context());

    TransientTextures transients(device);
    RenderGraph graph(transients);
    const RenderGraph::TextureId hdr = graph.create({
        .width = kSize,
        .height = kSize,
        .format = kHdrColorFormat,
        .usage = rhi::kColorTarget | rhi::kSampled,
    });
    const RenderGraph::TextureId target = graph.import(*final);
    graph.addPass({
        .name = "half white",
        .color = {hdr, rhi::LoadOp::Clear},
        .clearColor = Vec4(0.0f, 0.0f, 0.0f, 1.0f),
        .record = [&](rhi::CommandList& commands, const RenderGraph&) {
            commands.bindPipeline(*pipeline);
            commands.draw(3);
        },
    });
    output.addTo(graph, hdr, target);
    graph.execute(device.commands());

    // Линейная середина 0,5 в sRGB - 0,735, то есть 188 из 255; смешивание в
    // sRGB дало бы 128.
    const Pixel center = pixelAt(device.readPixels(*final), kSize / 2, kSize / 2, kSize);
    CHECK(std::abs(int(center[0]) - 188) <= 1);
}
