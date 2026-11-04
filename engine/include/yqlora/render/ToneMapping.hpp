#pragma once

#include "yqlora/reflection/EnumNames.hpp"

#include <cstdint>

namespace yqlora {

// Как проход вывода сжимает яркость кадра (после экспозиции) в то, что
// показывает экран. Порядок значений не важен: файлы и шейдер берут имена.
enum class ToneMapping : std::uint8_t {
    // Мягко гасит насыщенность ярких цветов, как плёнка: по умолчанию.
    AgX,
    // Подогнанная кривая ACES (RRT и ODT): контрастнее, яркое уходит в оттенок.
    Aces,
    // Khronos PBR Neutral: цвета до 0.76 не трогает, сжимает только яркое.
    Neutral,
    // Без сжатия: всё выше 1 обрезается.
    None,
};

} // namespace yqlora

YQLORA_ENUM_NAMES(yqlora::ToneMapping, "AgX", "Aces", "Neutral", "None")
