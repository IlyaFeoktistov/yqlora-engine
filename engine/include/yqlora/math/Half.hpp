#pragma once

#include <cstdint>

// glm/gtc/packing.hpp сыплет предупреждениями о сужении в своих упаковках
// 10:10:10:2, которыми движок не пользуется; они не о нашем коде.
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#endif
#include <glm/gtc/packing.hpp>
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace yqlora::math {

// Половинный float (IEEE 754 binary16), как в текстурах Rgba16Float.
inline std::uint16_t toHalf(float value)
{
    return glm::packHalf1x16(value);
}

inline float fromHalf(std::uint16_t half)
{
    return glm::unpackHalf1x16(half);
}

} // namespace yqlora::math
