#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/scene/Layers.hpp"

#include <cstdint>

namespace yqlora {

class LayerCollisionMatrix;

// Слои Jolt из слоёв узлов. Слой широкой фазы Jolt берёт из слоя объекта,
// поэтому слой объекта несёт и подвижность: слой узла * 2 + (подвижный ? 1 : 0).
// Два слоя широкой фазы - неподвижный и подвижный, как советует Jolt.
using PhysicsObjectLayer = std::uint16_t;

enum class PhysicsBroadPhase : std::uint8_t {
    Static,
    Moving,
};

inline constexpr int kPhysicsMotionKinds = 2;
inline constexpr int kPhysicsObjectLayerCount = kLayerCount * kPhysicsMotionKinds;
inline constexpr int kPhysicsBroadPhaseCount = 2;

constexpr PhysicsObjectLayer physicsObjectLayer(int nodeLayer, bool moving)
{
    return static_cast<PhysicsObjectLayer>(nodeLayer * kPhysicsMotionKinds + (moving ? 1 : 0));
}

constexpr int nodeLayerOf(PhysicsObjectLayer layer)
{
    return layer / kPhysicsMotionKinds;
}

constexpr bool isMovingLayer(PhysicsObjectLayer layer)
{
    return layer % kPhysicsMotionKinds != 0;
}

constexpr PhysicsBroadPhase broadPhaseOf(PhysicsObjectLayer layer)
{
    return isMovingLayer(layer) ? PhysicsBroadPhase::Moving : PhysicsBroadPhase::Static;
}

// Сталкиваются ли объекты двух слоёв: неподвижное с неподвижным - никогда,
// остальное - по матрице слоёв узлов.
YQLORA_API bool physicsLayersCollide(const LayerCollisionMatrix& matrix, PhysicsObjectLayer a, PhysicsObjectLayer b);

// Нужно ли объекту слоя layer искать пары в слое широкой фазы: неподвижному
// - только среди подвижных.
YQLORA_API bool physicsLayerMeetsBroadPhase(PhysicsObjectLayer layer, PhysicsBroadPhase broadPhase);

} // namespace yqlora
