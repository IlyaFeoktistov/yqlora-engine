#pragma once

#include "yqlora/render/TextureData.hpp"
#include "yqlora/resource/ResourceLoader.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace yqlora {

// Картинка из байтов файла (PNG, JPG, TGA, BMP, HDR) в TextureData, как её
// читает ImageLoader: так же читаются картинки из буфера модели. sampling -
// как её брать; у HDR цветовое пространство всегда линейное. nullopt и
// причина в error, если байты не картинка.
YQLORA_API std::optional<TextureData> decodeImage(
    std::string_view bytes,
    const TextureSampling& sampling,
    std::string& error
);

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
