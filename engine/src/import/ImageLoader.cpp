#include "yqlora/import/ImageLoader.hpp"

#include "yqlora/import/TextureImport.hpp"
#include "yqlora/math/Half.hpp"
#include "yqlora/platform/Files.hpp"

#include <stb_image.h>

#include <array>
#include <cstring>
#include <limits>
#include <memory>
#include <vector>

namespace yqlora {

namespace {

constexpr std::array<std::string_view, 6> kExtensions{".png", ".jpg", ".jpeg", ".tga", ".bmp", ".hdr"};
constexpr int kRgba = 4;

struct ImageDeleter {
    void operator()(void* pixels) const
    {
        stbi_image_free(pixels);
    }
};

std::size_t pixelCount(int width, int height)
{
    return static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
}

// 8 бит на канал: цвет или данные, как скажут настройки импорта.
bool loadLdr(const stbi_uc* bytes, int size, TextureData& texture)
{
    int channels = 0;
    const std::unique_ptr<stbi_uc, ImageDeleter> pixels(
        stbi_load_from_memory(bytes, size, &texture.width, &texture.height, &channels, kRgba)
    );
    if (!pixels) {
        return false;
    }
    texture.format = PixelFormat::Rgba8;
    texture.pixels.assign(pixels.get(), pixels.get() + pixelCount(texture.width, texture.height) * kRgba);

    return true;
}

// HDR (Radiance .hdr): линейные float, хранятся половинными. Цветового
// пространства у них нет: значения уже линейные.
bool loadHdr(const stbi_uc* bytes, int size, TextureData& texture)
{
    int channels = 0;
    const std::unique_ptr<float, ImageDeleter> pixels(
        stbi_loadf_from_memory(bytes, size, &texture.width, &texture.height, &channels, kRgba)
    );
    if (!pixels) {
        return false;
    }
    texture.format = PixelFormat::Rgba16Float;
    const std::size_t values = pixelCount(texture.width, texture.height) * kRgba;
    texture.pixels.resize(values * sizeof(std::uint16_t));
    for (std::size_t i = 0; i < values; ++i) {
        const std::uint16_t half = math::toHalf(pixels.get()[i]);
        std::memcpy(texture.pixels.data() + i * sizeof(half), &half, sizeof(half));
    }
    texture.sampling.colorSpace = ColorSpace::Linear;

    return true;
}

} // namespace

std::span<const std::string_view> ImageLoader::extensions() const
{
    return kExtensions;
}

std::string_view ImageLoader::kind() const
{
    return TextureData::kResourceKind;
}

LoadResult ImageLoader::load(const LoadRequest& request) const
{
    const std::string bytes = platform::readFile(request.file).value_or(std::string());
    if (bytes.empty() || bytes.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return loadError("the file is empty, too large or not readable");
    }
    const auto* data = reinterpret_cast<const stbi_uc*>(bytes.data());
    const auto size = static_cast<int>(bytes.size());
    TextureData texture;
    texture.sampling = textureSamplingFromJson(request.import);
    const bool loaded = stbi_is_hdr_from_memory(data, size) != 0 ? loadHdr(data, size, texture)
                                                                 : loadLdr(data, size, texture);
    if (!loaded) {
        return loadError(stbi_failure_reason());
    }
    LoadResult result;
    result.resources.push_back(makeLoadedResource(std::string(), std::move(texture)));

    return result;
}

} // namespace yqlora
