#pragma once

// Только для кода движка, который включает Jolt (Jolt.h - раньше этого файла).

#include "yqlora/physics/PhysicsMaterialData.hpp"

#include <Jolt/Physics/Collision/PhysicsMaterial.h>

namespace JPH {

class Body;
class SubShapeID;

} // namespace JPH

namespace yqlora {

// Физический материал на фигурах Jolt: каждая фигура коллайдера несёт
// материал своего коллайдера, а функции сведения мира читают материалы
// обеих подфигур контакта. Все материалы фигур - этого типа, в том числе
// PhysicsMaterial::sDefault (JoltSetup), поэтому сведение приводит их без
// проверки.
class JoltPhysicsMaterial final : public JPH::PhysicsMaterial {
public:
    explicit JoltPhysicsMaterial(const PhysicsMaterialData& data)
        : m_data(data)
    {
    }

    const PhysicsMaterialData& data() const
    {
        return m_data;
    }

    // Только главный поток и только между шагами: шаг читает значения из
    // рабочих потоков.
    void setData(const PhysicsMaterialData& data)
    {
        m_data = data;
    }

private:
    PhysicsMaterialData m_data;
};

// Функции сведения мира (PhysicsSystem::SetCombineFriction и
// SetCombineRestitution). Jolt вызывает их из рабочих потоков для каждого
// контакта: они только читают материалы.
float combineJoltFriction(
    const JPH::Body& first,
    const JPH::SubShapeID& firstPart,
    const JPH::Body& second,
    const JPH::SubShapeID& secondPart
);
float combineJoltRestitution(
    const JPH::Body& first,
    const JPH::SubShapeID& firstPart,
    const JPH::Body& second,
    const JPH::SubShapeID& secondPart
);

} // namespace yqlora
