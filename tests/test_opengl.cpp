// Небольшие проверки на настоящем OpenGL: скрытое окно ради контекста. Где
// контекст не создать (сервер сборки без экрана), тесты пропускаются с
// сообщением.

#include <doctest/doctest.h>

#include <yqlora/editor/render/GridPass.hpp>
#include <yqlora/editor/render/OutlinePass.hpp>
#include <yqlora/platform/Platform.hpp>
#include <yqlora/platform/Window.hpp>
#include <yqlora/render/GraphicsBackend.hpp>
#include <yqlora/render/MeshData.hpp>
#include <yqlora/render/Prepare.hpp>
#include <yqlora/render/PrimitiveMeshes.hpp>
#include <yqlora/render/Renderer.hpp>
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

    auto target = renderer.createRenderTarget(kSize, kSize);
    renderer.render(frame, *target);
    const std::vector<std::byte> pixels = renderer.device().readPixels(target->color());

    // Посередине - освещённый красный куб, в углу - фон, вокруг куба - контур.
    const Pixel center = pixelAt(pixels, kSize / 2, kSize / 2, kSize);
    CHECK(center[0] > 50);
    CHECK(center[1] == 0);
    CHECK(center[2] == 0);
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
    CHECK(center[0] == 0);
    CHECK(center[1] > 50);
}
