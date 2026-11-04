#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/reflection/EnumNames.hpp"
#include "yqlora/resource/ResourceKinds.hpp"
#include <cstdint>
#include <string_view>
#include <vector>

namespace yqlora {

// Как хранятся цвета текстуры. Цвет (альбедо, иконки) - sRGB: при чтении в
// шейдере он переводится в линейный. Данные (нормали, шероховатость,
// металличность) - Linear, их числа не переводятся.
enum class ColorSpace {
    Srgb,
    Linear,
};

enum class TextureFilter {
    Linear,
    Nearest, // пиксельная графика
};

enum class TextureWrap {
    Repeat,
    Clamp,
    Mirror,
};

// Как текстуру читать: настройки импорта, а не свойства материала.
struct TextureSampling {
    ColorSpace colorSpace = ColorSpace::Srgb;
    TextureFilter filter = TextureFilter::Linear;
    TextureWrap wrap = TextureWrap::Repeat;
    bool mipmaps = true;

    bool operator==(const TextureSampling&) const = default;
};

// Как лежат пиксели TextureData.
enum class PixelFormat {
    // По байту на канал; цвет или данные, см. ColorSpace.
    Rgba8,
    // Половинные float по каналу, линейные: HDR-картинки (.hdr), яркость
    // может быть больше 1.
    Rgba16Float,
};

// Картинка на стороне CPU: RGBA, строки сверху вниз. Так же, как uv0 в
// glTF: (0, 0) - левый верхний угол.
struct TextureData {
    static constexpr std::string_view kResourceKind = kinds::kTexture;

    int width = 0;
    int height = 0;
    PixelFormat format = PixelFormat::Rgba8;
    // Плотно, без выравнивания строк, в формате format.
    std::vector<std::uint8_t> pixels{};
    TextureSampling sampling{};
};

// Пиксели для показа человеку (превью, иконка окна): RGBA8 в sRGB. HDR
// обрезается до 1 и кодируется в sRGB; 8-битные отдаются как есть.
YQLORA_API std::vector<std::uint8_t> displayPixels(const TextureData& texture);

} // namespace yqlora

YQLORA_ENUM_NAMES(yqlora::ColorSpace, "sRGB", "Linear")
YQLORA_ENUM_NAMES(yqlora::TextureFilter, "Linear", "Nearest")
YQLORA_ENUM_NAMES(yqlora::TextureWrap, "Repeat", "Clamp", "Mirror")
