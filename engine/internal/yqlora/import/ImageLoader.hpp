#pragma once

#include "yqlora/resource/ResourceLoader.hpp"

namespace yqlora {

// Картинки (PNG, JPG, TGA, BMP, HDR) в TextureData, всегда RGBA: 8 бит на
// канал, а HDR - половинные float (PixelFormat::Rgba16Float), всегда
// линейные. Настройки импорта - TextureSampling (textureSamplingFromJson).
class YQLORA_API ImageLoader final : public ResourceLoader {
public:
    std::span<const std::string_view> extensions() const override;
    std::string_view kind() const override;
    LoadResult load(const LoadRequest& request) const override;
};

} // namespace yqlora
