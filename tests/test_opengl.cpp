// Небольшие проверки на настоящем OpenGL: скрытое окно ради контекста. Где
// контекст не создать (сервер сборки без экрана), тесты пропускаются с
// сообщением.

#include "TestFrames.hpp"

#include <doctest/doctest.h>

#include <yqlora/editor/render/GridPass.hpp>
#include <yqlora/editor/render/OutlinePass.hpp>
#include <yqlora/platform/Platform.hpp>
#include <yqlora/platform/Window.hpp>
#include <yqlora/render/EngineUniforms.hpp>
#include <yqlora/render/FrameFormats.hpp>
#include <yqlora/render/FrameMaterials.hpp>
#include <yqlora/render/GraphicsBackend.hpp>
#include <yqlora/render/MeshData.hpp>
#include <yqlora/render/OutputPass.hpp>
#include <yqlora/render/Prepare.hpp>
#include <yqlora/render/PrimitiveMeshes.hpp>
#include <yqlora/render/RenderGraph.hpp>
#include <yqlora/render/Renderer.hpp>
#include <yqlora/render/TextureData.hpp>
#include <yqlora/render/ToneMapping.hpp>
#include <yqlora/render/ViewPass.hpp>
#include <yqlora/resource/BuiltinResources.hpp>
#include <yqlora/resource/ResourceCache.hpp>
#include <yqlora/shader/EngineShaders.hpp>
#include <yqlora/shader/ShaderBuild.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

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

// Белый направленный свет вдоль dir в пи люкс: повёрнутая к нему белая
// рассеивающая поверхность имеет яркость 1, а вид без камеры - экспозицию 1.
LightData directional(const Vec3& dir)
{
    return {.kind = LightKind::Directional, .direction = glm::normalize(dir), .color = Vec3(glm::pi<float>())};
}

// Свет со стороны камеры (она на +Z и смотрит на -Z): освещает то, что камера видит.
const LightData kFrontLight = directional(Vec3(0.0f, 0.0f, -1.0f));

// Куб в начале координат перед камерой cameraAtZ(3) в кадре с этим светом;
// цвет пикселя посередине.
Pixel litCubeCenter(Renderer& renderer, const std::vector<LightData>& lights)
{
    const MeshData cube = makeCube();
    test::TestMaterials materials;
    FrameData frame;
    frame.views.push_back({.camera = cameraAtZ(3.0f)});
    frame.drawItems.push_back(
        {.mesh = &cube,
         .meshId = 1,
         .indexCount = static_cast<std::uint32_t>(cube.indices.size()),
         .bounds = cube.bounds,
         .material = materials.add(frame)}
    );
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
    test::TestMaterials materials;
    FrameData frame;
    frame.views.push_back({.camera = cameraAtZ(3.0f), .background = background, .passes = {&grid, &outline}});
    frame.drawItems.push_back({
        .mesh = &cube,
        .meshId = 1,
        .indexCount = static_cast<std::uint32_t>(cube.indices.size()),
        .bounds = cube.bounds,
        .material = materials.add(frame, Vec4(1.0f, 0.0f, 0.0f, 1.0f)),
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
    test::TestMaterials materials;
    FrameData frame;
    frame.views.push_back({.camera = cameraAtZ(4.0f)});
    frame.lights.push_back(kFrontLight);
    frame.drawItems.push_back({
        .mesh = &cube,
        .meshId = 1,
        .indexCount = static_cast<std::uint32_t>(cube.indices.size()),
        .world = near,
        .bounds = transformBounds(cube.bounds, near),
        .material = materials.add(frame, Vec4(0.0f, 1.0f, 0.0f, 1.0f)),
    });
    frame.drawItems.push_back({
        .mesh = &cube,
        .meshId = 1,
        .indexCount = static_cast<std::uint32_t>(cube.indices.size()),
        .world = far,
        .bounds = transformBounds(cube.bounds, far),
        .material = materials.add(frame, Vec4(1.0f, 0.0f, 0.0f, 1.0f)),
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
    output.addTo(graph, hdr, target, ToneMapping::None);
    graph.execute(device.commands());

    // Линейная середина 0,5 в sRGB - 0,735, то есть 188 из 255; смешивание в
    // sRGB дало бы 128.
    const Pixel center = pixelAt(device.readPixels(*final), kSize / 2, kSize / 2, kSize);
    CHECK(std::abs(int(center[0]) - 188) <= 1);
}

namespace {

// Куб перед камерой материалом frame.materials[material] (с заменами
// overrides, если есть); цвет пикселя посередине при свете спереди.
Pixel cubeCenter(
    Renderer& renderer,
    FrameData& frame,
    std::size_t material,
    std::size_t overrides = DrawItem::kNoOverrides
)
{
    static const MeshData cube = makeCube();
    frame.views = {{.camera = cameraAtZ(3.0f)}};
    frame.lights = {kFrontLight};
    frame.drawItems = {{
        .mesh = &cube,
        .meshId = 1,
        .indexCount = static_cast<std::uint32_t>(cube.indices.size()),
        .bounds = cube.bounds,
        .material = material,
        .overrides = overrides,
    }};
    auto target = renderer.createRenderTarget(kSize, kSize);
    renderer.render(frame, *target);

    return pixelAt(renderer.device().readPixels(target->color()), kSize / 2, kSize / 2, kSize);
}

} // namespace

TEST_CASE("OpenGL: a texture of a material imported as sRGB is decoded when read")
{
    const auto gl = openGl();
    if (!gl) {
        return;
    }
    Renderer& renderer = *gl->renderer;
    // Серый 0.5 в sRGB: цветом материала и тем же серым из текстуры sRGB.
    constexpr float kGray = 0.5f;
    test::TestMaterials materials;
    FrameData byColor;
    const Pixel colored = cubeCenter(renderer, byColor, materials.add(byColor, Vec4(Vec3(kGray), 1.0f)));

    TextureData gray{.width = 1, .height = 1, .pixels = {toByte(kGray), toByte(kGray), toByte(kGray), 255}};
    FrameData byTexture;
    const std::size_t textured = materials.add(byTexture);
    for (FrameTexture& texture : byTexture.materials[textured].textures) {
        if (texture.slot.binding == byTexture.materials[textured].shader->findTexture("baseColorMap")->slot.binding) {
            texture = {.slot = texture.slot, .id = 900, .revision = 1, .data = &gray};
        }
    }
    const Pixel sampled = cubeCenter(renderer, byTexture, textured);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        CHECK(std::abs(int(sampled[channel]) - int(colored[channel])) <= 2);
    }
    // Без декодирования серый вышел бы заметно светлее. Новая ревизия - новая копия.
    gray.sampling.colorSpace = ColorSpace::Linear;
    for (FrameTexture& texture : byTexture.materials[textured].textures) {
        texture.revision = 2;
    }
    CHECK(int(cubeCenter(renderer, byTexture, textured)[0]) > int(colored[0]) + 20);
}

TEST_CASE("OpenGL: overrides change one of two objects with the same material")
{
    const auto gl = openGl();
    if (!gl) {
        return;
    }
    Renderer& renderer = *gl->renderer;
    test::TestMaterials materials;
    FrameData frame;
    const std::size_t white = materials.add(frame);
    FrameOverrides red{.textures = frame.materials[white].textures};
    red.values.setColor("baseColor", Vec4(1.0f, 0.0f, 0.0f, 1.0f));
    frame.overrides.push_back(red);

    const Pixel plain = cubeCenter(renderer, frame, white);
    const Pixel overridden = cubeCenter(renderer, frame, white, 0);
    CHECK(int(plain[1]) > 200);
    CHECK(int(overridden[0]) > int(overridden[1]) + kDominantMargin);
    // Общий материал не изменился.
    CHECK(cubeCenter(renderer, frame, white) == plain);
}

namespace {

// Линейная яркость канала по байту sRGB итоговой цели.
float linearOf(std::uint8_t value)
{
    constexpr float kMax = 255.0f;

    return math::srgbToLinear(float(value) / kMax);
}

// Совсем шероховатый белый неметалл: отражает почти только рассеянно (4
// процента Френеля дают блик около 1 процента яркости при свете спереди).
const std::map<std::string, std::vector<float>> kRough{{"roughness", {1.0f}}};

// Передняя грань куба cameraAtZ(3) с этим светом и экспозицией; яркость
// посередине в линейном RGB (красный канал, свет белый).
float frontFaceLuminance(Renderer& renderer, const LightData& light, float exposure)
{
    static const MeshData cube = makeCube();
    test::TestMaterials materials;
    FrameData frame;
    frame.views.push_back({.camera = cameraAtZ(3.0f), .exposure = {.factor = exposure}});
    frame.drawItems.push_back({
        .mesh = &cube,
        .meshId = 1,
        .indexCount = static_cast<std::uint32_t>(cube.indices.size()),
        .bounds = cube.bounds,
        .material = materials.add(frame, Vec4(1.0f), MaterialBlend::Opaque, kRough),
    });
    frame.lights = {light};
    auto target = renderer.createRenderTarget(kSize, kSize);
    renderer.render(frame, *target);

    return linearOf(pixelAt(renderer.device().readPixels(target->color()), kSize / 2, kSize / 2, kSize)[0]);
}

} // namespace

TEST_CASE("OpenGL: a white surface facing E lux has the luminance E / pi")
{
    const auto gl = openGl();
    if (!gl) {
        return;
    }
    constexpr float kLux = 1000.0f;
    // Экспозиция, при которой яркость E / пи выходит ровно в 0.5.
    constexpr float kHalf = 0.5f;
    const float exposure = kHalf * glm::pi<float>() / kLux;
    LightData sun{.kind = LightKind::Directional, .direction = Vec3(0.0f, 0.0f, -1.0f), .color = Vec3(kLux)};
    const float luminance = frontFaceLuminance(*gl->renderer, sun, exposure);
    // Рассеянная часть - (1 - 0.04) / пи, блик добавляет около 1 процента.
    CHECK(luminance > kHalf * 0.95f);
    CHECK(luminance < kHalf * 1.0f);
}

TEST_CASE("OpenGL: a point light of I candela gives I / d^2 lux")
{
    const auto gl = openGl();
    if (!gl) {
        return;
    }
    constexpr float kCandela = 100.0f;
    constexpr float kExposure = 0.02f;
    constexpr float kFrontFace = 0.5f;
    LightData point{.kind = LightKind::Point, .color = Vec3(kCandela), .range = 100.0f};
    point.position = Vec3(0.0f, 0.0f, kFrontFace + 1.0f);
    const float near = frontFaceLuminance(*gl->renderer, point, kExposure);
    point.position = Vec3(0.0f, 0.0f, kFrontFace + 2.0f);
    const float far = frontFaceLuminance(*gl->renderer, point, kExposure);
    // 100 лк на метре, 25 на двух.
    CHECK(near / far == doctest::Approx(4.0f).epsilon(0.05));
    CHECK(near == doctest::Approx(kCandela / glm::pi<float>() * kExposure).epsilon(0.05));
}

TEST_CASE("OpenGL: tone mapping keeps black, grows with luminance and does not clip at 1")
{
    const auto gl = openGl();
    if (!gl) {
        return;
    }
    Renderer& renderer = *gl->renderer;
    rhi::Device& device = renderer.device();
    // Слева направо яркость от 2^-8 до 2^8.
    const std::string fragment = "#version 450\n"
                                 "layout(location = 0) in vec2 vUv;\n"
                                 "layout(location = 0) out vec4 fragColor;\n"
                                 "void main() { fragColor = vec4(vec3(exp2(vUv.x * 16.0 - 8.0)), 1.0); }\n";
    ShaderCompiler compiler([&](const std::string& path) -> std::optional<std::string> {
        return path == "test/ramp.frag" ? std::optional(fragment) : std::nullopt;
    });
    auto shader = device.createShader(buildBuiltinShader(
        compiler,
        {.name = "ramp", .vertexPath = engineShaderPath("fullscreen.vert"), .fragmentPath = "test/ramp.frag"}
    ));
    auto pipeline = device.createPipeline({
        .shader = shader.get(),
        .depthTest = false,
        .depthWrite = false,
        .cull = rhi::CullMode::None,
        .colorFormat = kHdrColorFormat,
        .depthFormat = rhi::Format::None,
    });
    const OutputPass output(renderer.context());
    // Пиксель, где яркость 1: середина строки.
    constexpr int kOne = kSize / 2;

    for (const std::string_view name : EnumNames<ToneMapping>::names) {
        CAPTURE(name);
        const ToneMapping toneMapping = *enumFromName<ToneMapping>(name);
        auto final = device.createTexture(
            {.width = kSize, .height = kSize, .usage = rhi::kColorTarget | rhi::kSampled}
        );
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
            .name = "ramp",
            .color = {hdr, rhi::LoadOp::Clear},
            .record = [&](rhi::CommandList& commands, const RenderGraph&) {
                commands.bindPipeline(*pipeline);
                commands.draw(3);
            },
        });
        output.addTo(graph, hdr, target, toneMapping);
        graph.execute(device.commands());

        const std::vector<std::byte> pixels = device.readPixels(*final);
        int previous = 0;
        for (int x = 0; x < kSize; ++x) {
            const int value = pixelAt(pixels, x, kSize / 2, kSize)[0];
            CHECK(value >= previous);
            previous = value;
        }
        CHECK(pixelAt(pixels, 0, kSize / 2, kSize)[0] < 16);
        const int atOne = pixelAt(pixels, kOne, kSize / 2, kSize)[0];
        if (toneMapping == ToneMapping::None) {
            CHECK(atOne >= 254);
        } else {
            CHECK(atOne < 250);
        }
    }
}

TEST_CASE("OpenGL: depth compare LessEqual draws on the far plane, Less does not")
{
    const auto gl = openGl();
    if (!gl) {
        return;
    }
    rhi::Device& device = gl->renderer->device();
    const std::string vertex = "#version 450\n"
                               "void main() {\n"
                               "    vec2 uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);\n"
                               "    gl_Position = vec4(uv * 2.0 - 1.0, 1.0, 1.0);\n"
                               "}\n";
    const std::string fragment = "#version 450\n"
                                 "layout(location = 0) out vec4 fragColor;\n"
                                 "void main() { fragColor = vec4(1.0, 0.0, 0.0, 1.0); }\n";
    ShaderCompiler compiler([&](const std::string& path) -> std::optional<std::string> {
        if (path == "test/far.vert") {
            return vertex;
        }

        return path == "test/far.frag" ? std::optional(fragment) : std::nullopt;
    });
    auto shader = device.createShader(
        buildBuiltinShader(compiler, {.name = "far", .vertexPath = "test/far.vert", .fragmentPath = "test/far.frag"})
    );
    auto color = device.createTexture({.width = kSize, .height = kSize, .usage = rhi::kColorTarget | rhi::kSampled});
    auto depth = device.createTexture(
        {.width = kSize, .height = kSize, .format = kDepthFormat, .usage = rhi::kDepthTarget}
    );

    const auto drawn = [&](rhi::CompareOp compare) {
        auto pipeline = device.createPipeline({
            .shader = shader.get(),
            .depthWrite = false,
            .depthCompare = compare,
            .cull = rhi::CullMode::None,
        });
        rhi::CommandList& commands = device.commands();
        commands.beginRenderPass({.color = color.get(), .depth = depth.get()});
        commands.bindPipeline(*pipeline);
        commands.draw(3);
        commands.endRenderPass();

        return pixelAt(device.readPixels(*color), kSize / 2, kSize / 2, kSize)[0] == 255;
    };
    CHECK_FALSE(drawn(rhi::CompareOp::Less));
    CHECK(drawn(rhi::CompareOp::LessEqual));
}

namespace {

// Солнце в 100000 лк, к которому направление toSun.
LightData sunTowards(const Vec3& toSun)
{
    constexpr float kSunLux = 100000.0f;

    return {.kind = LightKind::Directional, .direction = -glm::normalize(toSun), .color = Vec3(kSunLux), .sun = true};
}

// Направление к солнцу на высоте elevation (градусы) над горизонтом, по
// азимуту along (единичный вектор в плоскости земли).
Vec3 sunAt(float elevationDegrees, const Vec3& along)
{
    const float elevation = glm::radians(elevationDegrees);

    return along * std::cos(elevation) + Vec3(0.0f, std::sin(elevation), 0.0f);
}

// Кадр с окружением: небо Default Sky и атмосфера Земли; камера на высоте
// человека смотрит вдоль -Z, без компрессии, с экспозицией exposure.
struct SkyFrame {
    ResourceCache cache;
    test::TestMaterials materials;
    FrameData frame;

    SkyFrame(const Vec3& toSun, float exposure, float far = 100.0f)
    {
        constexpr float kEyeHeight = 1.7f;
        const Vec3 eye(0.0f, kEyeHeight, 0.0f);
        frame.views.push_back({
            .camera =
                {
                    .view = glm::lookAt(eye, eye + Vec3(0.0f, 0.0f, -1.0f), Vec3(0.0f, 1.0f, 0.0f)),
                    .projection = math::perspective(glm::radians(60.0f), 1.0f, 0.1f, far),
                    .position = eye,
                },
            .clear = CameraClear::Sky,
            .exposure = {.factor = exposure},
        });
        FrameMaterials frameMaterials(cache, frame);
        frame.environment = {.sky = frameMaterials.sky(builtin::kDefaultSkyMaterial), .atmosphere = Atmosphere{}};
        frame.lights = {sunTowards(toSun)};
    }

    // Куб материала material в мире world (с его границами).
    void addCube(const Mat4& world, std::size_t material)
    {
        static const MeshData cube = makeCube();
        frame.drawItems.push_back({
            .mesh = &cube,
            .meshId = 1,
            .indexCount = static_cast<std::uint32_t>(cube.indices.size()),
            .world = world,
            .bounds = transformBounds(cube.bounds, world),
            .material = material,
        });
    }

    std::vector<std::byte> render(Renderer& renderer)
    {
        auto target = renderer.createRenderTarget(kSize, kSize);
        renderer.render(frame, *target);

        return renderer.device().readPixels(target->color());
    }
};

// Линейный цвет пикселя.
Vec3 linearPixel(const std::vector<std::byte>& pixels, int x, int y)
{
    const Pixel pixel = pixelAt(pixels, x, y, kSize);

    return {linearOf(pixel[0]), linearOf(pixel[1]), linearOf(pixel[2])};
}

// Экспозиция, при которой голубое небо днём (несколько тысяч нит) выходит в
// середину диапазона.
constexpr float kDaylightExposure = 1.0f / 20000.0f;

} // namespace

TEST_CASE("OpenGL: a Sky camera draws the sky only where there are no objects")
{
    const auto gl = openGl();
    if (!gl) {
        return;
    }
    SkyFrame sky(sunAt(60.0f, Vec3(0.0f, 0.0f, 1.0f)), kDaylightExposure);
    const std::size_t red = sky.materials.add(sky.frame, Vec4(1.0f, 0.0f, 0.0f, 1.0f));
    sky.addCube(glm::translate(Mat4(1.0f), Vec3(0.0f, 1.7f, -3.0f)), red);
    const std::vector<std::byte> pixels = sky.render(*gl->renderer);

    // Посередине - куб, вверху - голубое небо.
    const Vec3 cube = linearPixel(pixels, kSize / 2, kSize / 2);
    CHECK(cube.r > cube.b * 4.0f);
    const Vec3 zenith = linearPixel(pixels, kSize / 2, 2);
    CHECK(zenith.b > zenith.r * 1.5f);
    CHECK(zenith.b > 0.02f);
}

TEST_CASE("OpenGL: the sky near the horizon is redder at sunset than at noon")
{
    const auto gl = openGl();
    if (!gl) {
        return;
    }
    // Солнце сбоку (по +X), вне кадра; пиксель чуть выше горизонта.
    const auto redness = [&](float elevation) {
        SkyFrame sky(sunAt(elevation, Vec3(1.0f, 0.0f, 0.0f)), kDaylightExposure * 4.0f);
        const Vec3 color = linearPixel(sky.render(*gl->renderer), kSize / 2, kSize / 2 - 3);
        REQUIRE(color.b > 0.0f);

        return color.r / color.b;
    };
    CHECK(redness(2.0f) > redness(60.0f) * 1.5f);
}

TEST_CASE("OpenGL: at sunset the sun through the atmosphere is dimmer and redder")
{
    const auto gl = openGl();
    if (!gl) {
        return;
    }
    // Солнце низко за камерой: белый куб перед ней освещён почти в лоб.
    const auto lit = [&](bool atmosphere) {
        SkyFrame sky(sunAt(3.0f, Vec3(0.0f, 0.0f, 1.0f)), kDaylightExposure);
        sky.frame.environment.atmosphere.enabled = atmosphere;
        sky.addCube(glm::translate(Mat4(1.0f), Vec3(0.0f, 1.7f, -3.0f)), sky.materials.add(sky.frame));

        return linearPixel(sky.render(*gl->renderer), kSize / 2, kSize / 2);
    };
    const Vec3 clear = lit(false);
    const Vec3 sunset = lit(true);
    CHECK(math::luminance(sunset) < math::luminance(clear) * 0.5f);
    REQUIRE(sunset.b > 0.0f);
    CHECK(sunset.r / sunset.b > clear.r / clear.b * 1.5f);
}

TEST_CASE("OpenGL: a far object is closer to the sky colour than a near one")
{
    const auto gl = openGl();
    if (!gl) {
        return;
    }
    constexpr float kFar = 100000.0f;
    SkyFrame sky(sunAt(60.0f, Vec3(0.0f, 0.0f, 1.0f)), kDaylightExposure, kFar);
    const std::size_t red = sky.materials.add(sky.frame, Vec4(1.0f, 0.0f, 0.0f, 1.0f));
    // Ближний слева в 5 м, дальний справа в 20 км и большой, чтобы его было видно.
    sky.addCube(glm::translate(Mat4(1.0f), Vec3(-1.5f, 1.7f, -5.0f)), red);
    sky.addCube(glm::scale(glm::translate(Mat4(1.0f), Vec3(6000.0f, 1.7f, -20000.0f)), Vec3(3000.0f)), red);
    const std::vector<std::byte> pixels = sky.render(*gl->renderer);
    // Проекции центров: x / z = -0.3 и 0.3 при tg(30) = 0.577.
    constexpr int kNearX = 15;
    constexpr int kFarX = 48;
    const Vec3 near = linearPixel(pixels, kNearX, kSize / 2);
    const Vec3 far = linearPixel(pixels, kFarX, kSize / 2);
    const Vec3 skyColor = linearPixel(pixels, kSize / 2, 2);
    // Оттенок без яркости: доли каналов.
    const auto hue = [](const Vec3& color) {
        return color / std::max(color.r + color.g + color.b, 1e-6f);
    };
    const float nearToSky = glm::distance(hue(near), hue(skyColor));
    const float farToSky = glm::distance(hue(far), hue(skyColor));
    CHECK(farToSky < nearToSky * 0.9f);
    // Без дымки они одного оттенка.
    sky.frame.environment.atmosphere.aerialPerspective = 0.0f;
    const std::vector<std::byte> clear = sky.render(*gl->renderer);
    CHECK(glm::distance(hue(linearPixel(clear, kFarX, kSize / 2)), hue(linearPixel(clear, kNearX, kSize / 2))) < 0.05f);
}
