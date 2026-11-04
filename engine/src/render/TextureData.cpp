#include "yqlora/render/TextureData.hpp"

#include "yqlora/math/Half.hpp"
#include "yqlora/math/Math.hpp"

#include <algorithm>
#include <cstring>

namespace yqlora {

namespace {

constexpr int kChannels = 4;
constexpr int kAlpha = 3;
constexpr float kChannelMax = 255.0f;

} // namespace

std::vector<std::uint8_t> displayPixels(const TextureData& texture)
{
    if (texture.format == PixelFormat::Rgba8) {
        return texture.pixels;
    }

    const std::size_t count = texture.pixels.size() / sizeof(std::uint16_t);
    std::vector<std::uint8_t> result(count);
    for (std::size_t i = 0; i < count; ++i) {
        std::uint16_t half = 0;
        std::memcpy(&half, texture.pixels.data() + i * sizeof(half), sizeof(half));
        float value = std::clamp(math::fromHalf(half), 0.0f, 1.0f);
        if (i % kChannels != kAlpha) {
            value = math::linearToSrgb(value);
        }
        result[i] = static_cast<std::uint8_t>(value * kChannelMax + 0.5f);
    }

    return result;
}

} // namespace yqlora
