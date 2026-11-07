#include "yqlora/import/OrmPacking.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace yqlora {

namespace {

constexpr int kChannels = 4;
constexpr std::uint8_t kFull = 255;
constexpr int kRed = 0;
constexpr int kGreen = 1;
constexpr int kBlue = 2;
constexpr int kAlpha = 3;

std::uint8_t texel(const TextureData& image, int x, int y, int channel)
{
    x = std::clamp(x, 0, image.width - 1);
    y = std::clamp(y, 0, image.height - 1);
    const auto at = (static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width) + static_cast<std::size_t>(x))
                        * kChannels
                    + channel;

    return image.pixels[at];
}

// Канал картинки в точке (u, v) 0..1 по центрам текселей, билинейно.
std::uint8_t sample(const TextureData& image, float u, float v, int channel)
{
    const float x = u * static_cast<float>(image.width) - 0.5f;
    const float y = v * static_cast<float>(image.height) - 0.5f;
    const float left = std::floor(x);
    const float top = std::floor(y);
    const float fx = x - left;
    const float fy = y - top;
    const int ix = static_cast<int>(left);
    const int iy = static_cast<int>(top);
    const float upper = std::lerp(float(texel(image, ix, iy, channel)), float(texel(image, ix + 1, iy, channel)), fx);
    const float lower = std::lerp(
        float(texel(image, ix, iy + 1, channel)),
        float(texel(image, ix + 1, iy + 1, channel)),
        fx
    );

    return static_cast<std::uint8_t>(std::lround(std::lerp(upper, lower, fy)));
}

bool usable(const TextureData* image)
{
    return !image || (image->format == PixelFormat::Rgba8 && image->width > 0 && image->height > 0);
}

} // namespace

std::optional<TextureData> packOrm(const TextureData* occlusion, const TextureData* metallicRoughness)
{
    if ((!occlusion && !metallicRoughness) || !usable(occlusion) || !usable(metallicRoughness)) {
        return std::nullopt;
    }
    TextureData packed;
    packed.width = std::max(occlusion ? occlusion->width : 0, metallicRoughness ? metallicRoughness->width : 0);
    packed.height = std::max(occlusion ? occlusion->height : 0, metallicRoughness ? metallicRoughness->height : 0);
    packed.format = PixelFormat::Rgba8;
    packed.sampling = metallicRoughness ? metallicRoughness->sampling : occlusion->sampling;
    packed.sampling.colorSpace = ColorSpace::Linear;
    packed.pixels.resize(static_cast<std::size_t>(packed.width) * static_cast<std::size_t>(packed.height) * kChannels);
    const auto same = [&](const TextureData& image) {
        return image.width == packed.width && image.height == packed.height;
    };
    for (int y = 0; y < packed.height; ++y) {
        for (int x = 0; x < packed.width; ++x) {
            const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(packed.width);
            const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(packed.height);
            const auto channel = [&](const TextureData* image, int from) {
                if (!image) {
                    return kFull;
                }

                return same(*image) ? texel(*image, x, y, from) : sample(*image, u, v, from);
            };
            const std::size_t at = (static_cast<std::size_t>(y) * static_cast<std::size_t>(packed.width)
                                    + static_cast<std::size_t>(x))
                                   * kChannels;
            packed.pixels[at + kRed] = channel(occlusion, kRed);
            packed.pixels[at + kGreen] = channel(metallicRoughness, kGreen);
            packed.pixels[at + kBlue] = channel(metallicRoughness, kBlue);
            packed.pixels[at + kAlpha] = kFull;
        }
    }

    return packed;
}

} // namespace yqlora
