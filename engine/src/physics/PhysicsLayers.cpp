#include "yqlora/physics/PhysicsLayers.hpp"

#include "yqlora/physics/LayerCollisionMatrix.hpp"

namespace yqlora {

bool physicsLayersCollide(const LayerCollisionMatrix& matrix, PhysicsObjectLayer a, PhysicsObjectLayer b)
{
    if (!isMovingLayer(a) && !isMovingLayer(b)) {
        return false;
    }

    return matrix.collides(nodeLayerOf(a), nodeLayerOf(b));
}

bool physicsLayerMeetsBroadPhase(PhysicsObjectLayer layer, PhysicsBroadPhase broadPhase)
{
    return isMovingLayer(layer) || broadPhase == PhysicsBroadPhase::Moving;
}

} // namespace yqlora
