#include "yqlora/physics/PhysicsChain.hpp"

#include "yqlora/physics/PhysicsWorldAccess.hpp"

#include <algorithm>
#include <cstddef>

namespace yqlora {

namespace {

float share(float grip)
{
    return std::clamp(grip, 0.0f, 1.0f);
}

} // namespace

PhysicsChain::PhysicsChain()
    : m_bends(static_cast<std::size_t>(m_segments - 1), Vec2(0.0f))
{
}

void PhysicsChain::setSegments(int segments)
{
    m_segments = std::clamp(segments, kMinChainSegments, kMaxChainSegments);
    m_bends.resize(static_cast<std::size_t>(m_segments - 1), Vec2(0.0f));
    changed();
}

void PhysicsChain::setLength(float length)
{
    m_length = std::max(length, kMinChainSize);
    changed();
}

void PhysicsChain::setRadius(float radius)
{
    m_radius = std::max(radius, kMinChainSize);
    changed();
}

void PhysicsChain::setMass(float mass)
{
    m_mass = std::max(mass, kMinChainSize);
    changed();
}

void PhysicsChain::setBendLimit(float degrees)
{
    m_bendLimit = std::clamp(degrees, 0.0f, kMaxChainBendDegrees);
    changed();
}

void PhysicsChain::setTwistLimit(float degrees)
{
    m_twistLimit = std::clamp(degrees, 0.0f, kMaxChainBendDegrees);
    changed();
}

void PhysicsChain::setMaterial(Ref<PhysicsMaterialData> material)
{
    m_material = material;
    changed();
}

void PhysicsChain::setStiffness(float stiffness)
{
    m_stiffness = std::max(stiffness, 0.0f);
    tuned();
}

void PhysicsChain::setDamping(float damping)
{
    m_damping = std::max(damping, 0.0f);
    tuned();
}

void PhysicsChain::setMaxTorque(float torque)
{
    m_maxTorque = std::max(torque, 0.0f);
    tuned();
}

void PhysicsChain::setSideGrip(float grip)
{
    m_sideGrip = share(grip);
}

void PhysicsChain::setForwardGrip(float grip)
{
    m_forwardGrip = share(grip);
}

void PhysicsChain::setBend(int index, float side, float up)
{
    if (index < 0 || index >= static_cast<int>(m_bends.size())) {
        return;
    }
    m_bends[static_cast<std::size_t>(index)] = {side, up};
    m_relaxed = false;
    tuned();
}

void PhysicsChain::setBends(const std::vector<Vec2>& bends)
{
    const std::size_t count = std::min(bends.size(), m_bends.size());
    std::copy_n(bends.begin(), count, m_bends.begin());
    m_relaxed = false;
    tuned();
}

Vec2 PhysicsChain::targetBend(int index) const
{
    if (index < 0 || index >= static_cast<int>(m_bends.size())) {
        return Vec2(0.0f);
    }

    return m_bends[static_cast<std::size_t>(index)];
}

void PhysicsChain::relax()
{
    m_relaxed = true;
    tuned();
}

void PhysicsChain::changed()
{
    PhysicsWorldAccess::changed(*this);
}

void PhysicsChain::tuned()
{
    PhysicsWorldAccess::tuned(*this);
}

void PhysicsChain::onEnterScene()
{
    PhysicsWorldAccess::changed(*this);
}

void PhysicsChain::onExitScene()
{
    PhysicsWorldAccess::left(*this);
}

} // namespace yqlora
