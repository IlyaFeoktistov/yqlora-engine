#pragma once

// Только для кода движка, который включает Jolt (Jolt.h - раньше этого файла).

#include "yqlora/physics/JoltMaterials.hpp"
#include "yqlora/resource/ResourceId.hpp"

#include <cstdint>
#include <unordered_map>

namespace yqlora {

class ResourceCache;

// Материалы Jolt из ресурсов PhysicsMaterialData: один объект на ID на
// всю жизнь среды, общий для всех фигур с этим материалом. Правка или
// перевыгрузка ресурса меняет значения этого объекта перед шагом (refresh),
// поэтому формы и тела не перестраиваются, а новые значения действуют со
// следующего шага. Ресурса нет или он не читается - значения
// PhysicsMaterialData{}. Только главный поток.
class PhysicsMaterialCache {
public:
    const JoltPhysicsMaterial* material(ResourceCache& resources, ResourceId id);

    // Перед шагом: значения материалов из их текущих ревизий.
    void refresh(ResourceCache& resources);

    // Растёт, когда refresh меняет значения любого материала: мир, который
    // видит новое число, будит свои тела, иначе спящие не заметили бы
    // нового трения.
    std::uint64_t generation() const
    {
        return m_generation;
    }

private:
    struct Entry {
        std::uint64_t revision = 0;
        JPH::Ref<JoltPhysicsMaterial> material;
    };

    // true, если значения поменялись.
    static bool update(ResourceCache& resources, ResourceId id, Entry& entry);

    std::unordered_map<ResourceId, Entry> m_entries;
    std::uint64_t m_generation = 0;
};

} // namespace yqlora
