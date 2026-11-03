#include "yqlora/import/ImageLoader.hpp"

#include "yqlora/import/TextureImport.hpp"
#include "yqlora/platform/Files.hpp"

#include <stb_image.h>

#include <array>
#include <limits>
#include <memory>
#include <vector>

namespace yqlora {

namespace {

constexpr std::array<std::string_view, 5> kExtensions{".png", ".jpg", ".jpeg", ".tga", ".bmp"};
constexpr int kRgba = 4;

struct ImageDeleter {
    void operator()(stbi_uc* pixels) const
    {
        stbi_image_free(pixels);
    }
};

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
    int width = 0;
    int height = 0;
    int channels = 0;
    const std::unique_ptr<stbi_uc, ImageDeleter> pixels(stbi_load_from_memory(
        reinterpret_cast<const stbi_uc*>(bytes.data()),
        static_cast<int>(bytes.size()),
        &width,
        &height,
        &channels,
        kRgba
    ));
    if (!pixels) {
        return loadError(stbi_failure_reason());
    }

    TextureData texture;
    texture.width = width;
    texture.height = height;
    const std::size_t size = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * kRgba;
    texture.pixels.assign(pixels.get(), pixels.get() + size);
    texture.sampling = textureSamplingFromJson(request.import);
    LoadResult result;
    result.resources.push_back(makeLoadedResource(std::string(), std::move(texture)));

    return result;
}

} // namespace yqlora
