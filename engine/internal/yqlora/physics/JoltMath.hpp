#pragma once

// Только для кода движка, который включает Jolt (Jolt.h - раньше этого файла).

#include "yqlora/math/Math.hpp"

#include <Jolt/Math/Quat.h>
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

// glm хранит кватернион как (w, x, y, z), Jolt - как (x, y, z, w).
inline JPH::Quat toJolt(const Quat& value)
{
    return {value.x, value.y, value.z, value.w};
}

inline Quat fromJolt(JPH::QuatArg value)
{
    return {value.GetW(), value.GetX(), value.GetY(), value.GetZ()};
}

} // namespace yqlora
