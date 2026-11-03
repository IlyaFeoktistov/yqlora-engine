#pragma once

#include "yqlora/reflection/EnumNames.hpp"
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

// Картинка на стороне CPU: RGBA по байту на канал, строки сверху вниз.
// Так же, как uv0 в glTF: (0, 0) - левый верхний угол.
struct TextureData {
    static constexpr std::string_view kResourceKind = "texture";

    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> pixels{};
    TextureSampling sampling{};
};

} // namespace yqlora

YQLORA_ENUM_NAMES(yqlora::ColorSpace, "sRGB", "Linear")
YQLORA_ENUM_NAMES(yqlora::TextureFilter, "Linear", "Nearest")
YQLORA_ENUM_NAMES(yqlora::TextureWrap, "Repeat", "Clamp", "Mirror")
