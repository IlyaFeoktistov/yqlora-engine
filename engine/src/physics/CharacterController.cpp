#include "yqlora/physics/CharacterController.hpp"

#include "yqlora/physics/PhysicsWorldAccess.hpp"

#include <algorithm>

namespace yqlora {

void CharacterController::setRadius(float radius)
{
    m_radius = std::max(radius, kMinCharacterSize);
    changed();
}

void CharacterController::setHeight(float height)
{
    m_height = std::max(height, kMinCharacterSize);
    changed();
}

void CharacterController::setCenter(const Vec3& center)
{
    m_center = center;
    changed();
}

void CharacterController::setSkinWidth(float width)
{
    m_skinWidth = std::max(width, 0.0f);
    changed();
}

// Остальное персонаж берёт в каждом move: перестраивать его незачем.

void CharacterController::setSlopeLimit(float degrees)
{
    m_slopeLimit = std::clamp(degrees, 0.0f, kMaxCharacterSlope);
}

void CharacterController::setStepHeight(float height)
{
    m_stepHeight = std::max(height, 0.0f);
}

void CharacterController::setWalkStairs(bool walk)
{
    m_walkStairs = walk;
}

void CharacterController::setStickToGround(bool stick)
{
    m_stickToGround = stick;
}

void CharacterController::setStickDistance(float distance)
{
    m_stickDistance = std::max(distance, 0.0f);
}

void CharacterController::setMass(float mass)
{
    m_mass = std::max(mass, kMinCharacterSize);
}

void CharacterController::setPushForce(float force)
{
    m_pushForce = std::max(force, 0.0f);
}

void CharacterController::changed()
{
    PhysicsWorldAccess::changed(*this);
}

void CharacterController::onEnterScene()
{
    PhysicsWorldAccess::changed(*this);
}

void CharacterController::onExitScene()
{
    PhysicsWorldAccess::left(*this);
}

} // namespace yqlora
