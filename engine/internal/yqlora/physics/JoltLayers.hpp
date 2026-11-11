#pragma once

// Только для кода движка, который включает Jolt (Jolt.h - раньше этого файла).

#include "yqlora/physics/LayerCollisionMatrix.hpp"
#include "yqlora/physics/PhysicsLayers.hpp"

#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>

namespace yqlora {

// Правила слоёв (PhysicsLayers) в виде интерфейсов, которые ждёт
// JPH::PhysicsSystem::Init. Система хранит ссылки на них, поэтому они
// живут дольше неё.

class JoltBroadPhaseLayers final : public JPH::BroadPhaseLayerInterface {
public:
    JPH::uint GetNumBroadPhaseLayers() const override;
    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override;
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override;
#endif
};

class JoltObjectVsBroadPhase final : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer layer, JPH::BroadPhaseLayer broadPhase) const override;
};

// Пары слоёв объектов по копии матрицы: мир ставит её перед шагом в главном
// потоке, потоки Jolt во время шага только читают.
class JoltObjectPairs final : public JPH::ObjectLayerPairFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override;

    LayerCollisionMatrix matrix;
};

} // namespace yqlora
