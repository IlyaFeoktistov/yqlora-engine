#pragma once

// Только для кода движка, который включает Jolt (Jolt.h - раньше этого файла).

#include <Jolt/Physics/Body/BodyID.h>

namespace JPH {

class BodyLockInterface;

} // namespace JPH

namespace yqlora {

// Масса тела Jolt, кг; 0 - тела нет или оно не динамическое (масса
// бесконечна).
float joltBodyMass(const JPH::BodyLockInterface& locks, JPH::BodyID id);

} // namespace yqlora
