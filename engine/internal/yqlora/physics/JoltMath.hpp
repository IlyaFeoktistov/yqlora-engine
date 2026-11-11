#pragma once

// Только для кода движка, который включает Jolt (Jolt.h - раньше этого файла).

#include "yqlora/math/Math.hpp"

#include <Jolt/Math/Vec3.h>

namespace yqlora {

inline JPH::Vec3 toJolt(const Vec3& value)
{
    return {value.x, value.y, value.z};
}

inline Vec3 fromJolt(JPH::Vec3Arg value)
{
    return {value.GetX(), value.GetY(), value.GetZ()};
}

} // namespace yqlora
