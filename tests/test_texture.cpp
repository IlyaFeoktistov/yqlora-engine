#include "EngineRegistry.hpp"
#include "TestFiles.hpp"

#include <doctest/doctest.h>

#include <yqlora/core/EngineServices.hpp>
#include <yqlora/editor/EditorSession.hpp>
#include <yqlora/editor/ProjectSetup.hpp>
#include <yqlora/editor/TextureInspection.hpp>
#include <yqlora/import/ImageLoader.hpp>
#include <yqlora/import/TextureImport.hpp>
#include <yqlora/input/Input.hpp>
#include <yqlora/project/GeneralSettings.hpp>
#include <yqlora/project/Project.hpp>
#include <yqlora/render/TextureData.hpp>
#include <yqlora/render/TextureSampler.hpp>
#include <yqlora/resource/ResourceCache.hpp>
#include <yqlora/scene/SceneSerializer.hpp>
#include <yqlora/scripting/GameModule.hpp>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#include <stb_image_write.h>

#include <yqlora/math/Half.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace yqlora;
namespace fs = std::filesystem;
using Json = nlohmann::json;

namespace {

constexpr int kRgba = 4;
constexpr int kJpegQuality = 100;

// 2 x 2: красный, зелёный сверху; синий, полупрозрачный белый снизу.
const std::array<std::uint8_t, 16> kPixels{
    255,
    0,
    0,
    255,
    0,
    255,
    0,
    255, //
    0,
    0,
    255,
    255,
    255,
    255,
    255,
    128,
};

void appendBytes(void* context, void* data, int size)
{
    auto* bytes = static_cast<std::vector<char>*>(context);
    bytes->insert(bytes->end(), static_cast<char*>(data), static_cast<char*>(data) + size);
}

void save(const fs::path& file, const std::vector<char>& bytes)
{
    std::ofstream(file, std::ios::binary).write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

void writePng(const fs::path& file)
{
    std::vector<char> bytes;
    stbi_write_png_to_func(&appendBytes, &bytes, 2, 2, kRgba, kPixels.data(), 2 * kRgba);
    save(file, bytes);
}

// JPG без прозрачности: одноцветный, чтобы сжатие не меняло цвет заметно.
void writeJpg(const fs::path& file)
{
    std::vector<std::uint8_t> gray(4 * 4 * 3, 200);
    std::vector<char> bytes;
    stbi_write_jpg_to_func(&appendBytes, &bytes, 4, 4, 3, gray.data(), kJpegQuality);
    save(file, bytes);
}

const TextureData& texture(const LoadResult& result)
{
    REQUIRE(result.error.empty());
    REQUIRE(result.resources.size() == 1);
    REQUIRE(result.resources[0].kind == TextureData::kResourceKind);

    return *static_cast<const TextureData*>(result.resources[0].data.get());
}

} // namespace

TEST_CASE("ImageLoader: PNG keeps alpha and rows go from the top")
{
    const fs::path file = test::freshDirectory("texture_png") / "four.png";
    writePng(file);
    const LoadResult result = ImageLoader{}.load({file});
    const TextureData& data = texture(result);
    CHECK(data.width == 2);
    CHECK(data.height == 2);
    CHECK(std::ranges::equal(data.pixels, kPixels));
    CHECK(data.sampling == TextureSampling{});
}

TEST_CASE("ImageLoader: JPG becomes RGBA, opaque")
{
    const fs::path file = test::freshDirectory("texture_jpg") / "gray.JPG";
    writeJpg(file);
    const LoadResult result = ImageLoader{}.load({file});
    const TextureData& data = texture(result);
    CHECK(data.width == 4);
    REQUIRE(data.pixels.size() == 4 * 4 * kRgba);
    CHECK(data.pixels[3] == 255);
    CHECK(data.pixels[0] > 190);
    CHECK(data.pixels[0] < 210);
}

TEST_CASE("ImageLoader: HDR keeps values above 1 as half floats, always linear")
{
    const fs::path file = test::freshDirectory("texture_hdr") / "sky.hdr";
    // 2 x 1 без сжатия: яркий (4) и тёмный (0.25) серые в RGBE, значение -
    // мантисса * 2^(порядок - 136): 128 * 2^-5 = 4, 128 * 2^-9 = 0.25.
    const std::string header = "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 1 +X 2\n";
    std::vector<char> bytes(header.begin(), header.end());
    for (const char exponent : {char(131), char(127)}) {
        bytes.insert(bytes.end(), {char(128), char(128), char(128), exponent});
    }
    save(file, bytes);

    const LoadResult result = ImageLoader{}.load({file});
    const TextureData& data = texture(result);
    CHECK(data.format == PixelFormat::Rgba16Float);
    CHECK(data.sampling.colorSpace == ColorSpace::Linear);
    REQUIRE(data.pixels.size() == 2 * kRgba * sizeof(std::uint16_t));
    const auto channel = [&](std::size_t index) {
        std::uint16_t half = 0;
        std::memcpy(&half, data.pixels.data() + index * sizeof(half), sizeof(half));

        return math::fromHalf(half);
    };
    CHECK(channel(0) == doctest::Approx(4.0f).epsilon(0.02));
    CHECK(channel(3) == doctest::Approx(1.0f));
    CHECK(channel(4) == doctest::Approx(0.25f).epsilon(0.02));

    // Для показа: яркое обрезается до белого, тёмное кодируется в sRGB.
    const std::vector<std::uint8_t> shown = displayPixels(data);
    REQUIRE(shown.size() == 2 * kRgba);
    CHECK(shown[0] == 255);
    CHECK(std::abs(int(shown[4]) - int(math::linearToSrgb(0.25f) * 255.0f + 0.5f)) <= 1);
}

TEST_CASE("ImageLoader: broken and missing files give an error")
{
    const fs::path directory = test::freshDirectory("texture_broken");
    std::ofstream(directory / "text.png") << "not a picture";
    CHECK_FALSE(ImageLoader{}.load({directory / "text.png"}).error.empty());
    CHECK_FALSE(ImageLoader{}.load({directory / "missing.png"}).error.empty());
}

TEST_CASE("TextureImport: settings go to JSON and back, defaults are not written, unknown values are defaults")
{
    CHECK(toJson(TextureSampling{}) == Json::object());
    const TextureSampling pixelArt{ColorSpace::Linear, TextureFilter::Nearest, TextureWrap::Clamp, false};
    const Json json = toJson(pixelArt);
    CHECK(json == Json{{"colorSpace", "Linear"}, {"filter", "Nearest"}, {"wrap", "Clamp"}, {"mipmaps", false}});
    CHECK(textureSamplingFromJson(json) == pixelArt);

    const TextureSampling odd = textureSamplingFromJson({{"filter", "Cubic"}, {"wrap", 3}, {"mipmaps", "yes"}});
    CHECK(odd == TextureSampling{});
    CHECK(textureSamplingFromJson(Json::array()) == TextureSampling{});
}

TEST_CASE("ResourceCache: a picture is a texture, new import settings give a new revision")
{
    const fs::path directory = test::freshDirectory("texture_cache");
    writePng(directory / "icon.png");
    test::EngineRegistry registry;
    REQUIRE(Project::create(directory, "Game", registry));
    auto project = Project::open(directory, registry);
    REQUIRE(project);
    Input input;
    EngineServices services{input, registry};
    services.setProject(&*project);
    ResourceCache& cache = services.resources();

    CHECK(cache.kindOf("icon.png") == TextureData::kResourceKind);
    CHECK(cache.kindOf("model.GLB") == "model");
    CHECK(cache.kindOf("notes.txt").empty());

    const ResourceId icon = project->resources().id("icon.png");
    const auto before = cache.get<TextureData>(icon);
    REQUIRE(before);
    CHECK(before.data->sampling.colorSpace == ColorSpace::Srgb);

    const TextureSampling linear{.colorSpace = ColorSpace::Linear};
    REQUIRE(project->resources().setImportSettings(icon, toJson(linear).dump()));
    REQUIRE(cache.reload(icon));
    const auto after = cache.get<TextureData>(icon);
    CHECK(after.revision > before.revision);
    CHECK(after.data->sampling.colorSpace == ColorSpace::Linear);

    // Иконка игры - ссылка на текстуру в разделе General.
    project->settings().get<GeneralSettings>().icon = {icon};
    REQUIRE(project->settings().saveModified());
    services.setProject(nullptr);
    const auto reopened = Project::open(directory, registry);
    CHECK(reopened->settings().get<GeneralSettings>().icon.id == icon);
}

TEST_CASE("Texture inspection: the picture and its import settings, applied with a new revision")
{
    test::EngineRegistry registry;
    SceneSerializer serializer{registry};
    std::string error;
    const auto created = editor::createProject(test::freshDirectory("texture_inspection"), "Game", serializer, error);
    REQUIRE(created);
    const fs::path directory = created->directory();
    writePng(directory / "icon.png");
    Input input;
    EngineServices services{input, registry};
    GameModule gameModule{registry};
    editor::EditorSession session{services, serializer, gameModule};
    REQUIRE(session.open(*Project::open(directory, registry)));
    const ResourceId icon = session.project()->resources().id("icon.png");

    const auto inspected = editor::inspectTexture(session, icon);
    REQUIRE(inspected);
    CHECK(inspected->texture.data->width == 2);
    CHECK(inspected->import == TextureSampling{});

    const TextureSampling pixelArt{.filter = TextureFilter::Nearest, .mipmaps = false};
    REQUIRE(editor::applyTextureImport(session, icon, pixelArt));
    const auto after = editor::inspectTexture(session, icon);
    CHECK(after->texture.revision > inspected->texture.revision);
    CHECK(after->import == pixelArt);
    CHECK(after->texture.data->sampling == pixelArt);
}

TEST_CASE("samplerDesc: the import settings of a texture as a GPU sampler")
{
    const rhi::SamplerDesc defaults = samplerDesc(TextureSampling{});
    CHECK(defaults.filter == rhi::Filter::Linear);
    CHECK(defaults.wrap == rhi::Wrap::Repeat);
    CHECK(defaults.mipmaps);
    const rhi::SamplerDesc pixelArt = samplerDesc(
        {.filter = TextureFilter::Nearest, .wrap = TextureWrap::Mirror, .mipmaps = false}
    );
    CHECK(pixelArt.filter == rhi::Filter::Nearest);
    CHECK(pixelArt.wrap == rhi::Wrap::Mirror);
    CHECK_FALSE(pixelArt.mipmaps);
    CHECK(samplerDesc({.wrap = TextureWrap::Clamp}).wrap == rhi::Wrap::Clamp);
}
