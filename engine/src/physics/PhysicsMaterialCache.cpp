// Jolt.h - первым: остальные заголовки Jolt на него опираются.
#include <Jolt/Jolt.h>

#include "yqlora/physics/PhysicsMaterialCache.hpp"

#include "yqlora/resource/ResourceCache.hpp"

namespace yqlora {

const JoltPhysicsMaterial* PhysicsMaterialCache::material(ResourceCache& resources, ResourceId id)
{
    Entry& entry = m_entries[id];
    if (!entry.material) {
        entry.material = new JoltPhysicsMaterial(PhysicsMaterialData{});
        update(resources, id, entry);
    }

    return entry.material.GetPtr();
}

void PhysicsMaterialCache::refresh(ResourceCache& resources)
{
    for (auto& [id, entry] : m_entries) {
        if (update(resources, id, entry)) {
            ++m_generation;
        }
    }
}

bool PhysicsMaterialCache::update(ResourceCache& resources, ResourceId id, Entry& entry)
{
    const LoadedData<PhysicsMaterialData> data = resources.get<PhysicsMaterialData>(id);
    if (data.revision == entry.revision && entry.revision != 0) {
        return false;
    }
    entry.revision = data.revision;
    const PhysicsMaterialData values = data ? sanitized(*data.data) : PhysicsMaterialData{};
    if (values == entry.material->data()) {
        return false;
    }
    entry.material->setData(values);

    return true;
}

} // namespace yqlora
