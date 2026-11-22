// Jolt.h - первым: остальные заголовки Jolt на него опираются.
#include <Jolt/Jolt.h>

#include "yqlora/physics/JoltBodyMass.hpp"

#include <Jolt/Physics/Body/BodyLock.h>

namespace yqlora {

float joltBodyMass(const JPH::BodyLockInterface& locks, JPH::BodyID id)
{
    if (id.IsInvalid()) {
        return 0.0f;
    }
    const JPH::BodyLockRead lock(locks, id);
    const JPH::MotionProperties* motion = lock.Succeeded() ? lock.GetBody().GetMotionPropertiesUnchecked() : nullptr;
    const float inverse = motion ? motion->GetInverseMass() : 0.0f;

    return inverse > 0.0f ? 1.0f / inverse : 0.0f;
}

} // namespace yqlora
