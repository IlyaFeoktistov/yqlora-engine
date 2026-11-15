#pragma once

// Только для кода движка, который включает Jolt (Jolt.h - раньше этого файла).

#include "yqlora/resource/ResourceId.hpp"

#include <Jolt/Physics/Collision/PhysicsMaterial.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>

#include <cstdint>
#include <map>
#include <tuple>

namespace yqlora {

class ResourceCache;

// Формы Jolt из мешей, приготовленные один раз на меш, его ревизию и
// физический материал и общие для всех коллайдеров с этим мешем и
// материалом: треугольники (MeshShape) или выпуклая оболочка
// (ConvexHullShape). Объект материала постоянный на всю жизнь среды
// (PhysicsMaterialCache), поэтому правка материала форму не меняет. Новая ревизия меша (перевыгрузка) готовит
// форму заново, а старая уходит, когда её не держит ни одно тело. Только
// главный поток.
class MeshShapeCache {
public:
    // nullptr - меша нет, он пуст или Jolt его не принял (тогда сообщение в лог).
    JPH::RefConst<JPH::Shape> shape(
        ResourceCache& resources,
        ResourceId mesh,
        bool convex,
        const JPH::PhysicsMaterial* material
    );

    // Ревизия меша, из которой форма готовилась бы сейчас; 0 - меша нет.
    static std::uint64_t revisionOf(ResourceCache& resources, ResourceId mesh);

    // Сколько раз форма готовилась: для проверок.
    std::uint64_t cooked() const
    {
        return m_cooked;
    }

private:
    struct Entry {
        std::uint64_t revision = 0;
        JPH::RefConst<JPH::Shape> shape;
    };

    std::map<std::tuple<ResourceId, bool, const JPH::PhysicsMaterial*>, Entry> m_entries;
    std::uint64_t m_cooked = 0;
};

} // namespace yqlora
