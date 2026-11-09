#pragma once

#include <cstdint>
#include <string_view>

namespace yqlora {

// 64-битный хэш строки (FNV-1a), одинаковый на любой машине и в любой сборке,
// в отличие от std::hash: из него выводятся ID, которые должны совпадать между
// запусками (узлы модели по их ключам).
constexpr std::uint64_t stableHash(std::string_view text)
{
    constexpr std::uint64_t kOffsetBasis = 0xCBF29CE484222325ULL;
    constexpr std::uint64_t kPrime = 0x100000001B3ULL;
    std::uint64_t hash = kOffsetBasis;
    for (const char c : text) {
        hash ^= static_cast<std::uint8_t>(c);
        hash *= kPrime;
    }

    return hash;
}

} // namespace yqlora
