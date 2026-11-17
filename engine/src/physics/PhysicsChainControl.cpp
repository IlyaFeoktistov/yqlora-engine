// Jolt.h - первым: остальные заголовки Jolt на него опираются.
#include <Jolt/Jolt.h>

// Состояние звеньев из кода: часть PhysicsChain, которой нужен мир Jolt.

#include "yqlora/physics/PhysicsChain.hpp"
#include "yqlora/physics/PhysicsWorldImpl.hpp"

namespace yqlora {

Vec2 PhysicsChain::bend(int index) const
{
    const PhysicsChains* chains = PhysicsWorldJolt::chainsOf(*this);

    return chains ? chains->bend(*this, index) : Vec2(0.0f);
}

Vec3 PhysicsChain::linkPosition(int index) const
{
    const PhysicsChains* chains = PhysicsWorldJolt::chainsOf(*this);

    return chains ? chains->linkPosition(*this, index) : Vec3(0.0f);
}

Vec3 PhysicsChain::linkVelocity(int index) const
{
    const PhysicsChains* chains = PhysicsWorldJolt::chainsOf(*this);

    return chains ? chains->linkVelocity(*this, index) : Vec3(0.0f);
}

} // namespace yqlora
