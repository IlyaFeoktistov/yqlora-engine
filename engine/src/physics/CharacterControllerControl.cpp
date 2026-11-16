// Jolt.h - первым: остальные заголовки Jolt на него опираются.
#include <Jolt/Jolt.h>

// Движение персонажа: часть CharacterController, которой нужен мир Jolt.

#include "yqlora/physics/CharacterController.hpp"
#include "yqlora/physics/PhysicsWorldImpl.hpp"

namespace yqlora {

namespace {

CharacterState stateOf(const CharacterController& controller)
{
    const PhysicsCharacters* characters = PhysicsWorldJolt::charactersOf(controller);

    return characters ? characters->state(controller) : CharacterState{};
}

} // namespace

void CharacterController::move(const Vec3& velocity, float deltaSeconds)
{
    if (PhysicsCharacters* characters = PhysicsWorldJolt::charactersOf(*this)) {
        characters->move(*this, velocity, deltaSeconds);
    }
}

bool CharacterController::isGrounded() const
{
    return stateOf(*this).grounded;
}

Vec3 CharacterController::groundNormal() const
{
    return stateOf(*this).groundNormal;
}

Vec3 CharacterController::groundVelocity() const
{
    return stateOf(*this).groundVelocity;
}

Vec3 CharacterController::velocity() const
{
    return stateOf(*this).velocity;
}

bool CharacterController::resize(float height)
{
    const float clamped = std::max(height, kMinCharacterSize);
    // Ноги остаются на месте: центр сдвигается на половину разницы высот.
    const Vec3 center = m_center - math::kWorldUp * ((m_height - clamped) * 0.5f);
    PhysicsCharacters* characters = PhysicsWorldJolt::charactersOf(*this);
    if (characters && !characters->tryCapsule(*this, clamped, center)) {
        return false;
    }
    m_height = clamped;
    m_center = center;

    return true;
}

} // namespace yqlora
