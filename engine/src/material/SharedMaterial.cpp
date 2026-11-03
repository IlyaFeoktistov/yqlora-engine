#include "yqlora/material/SharedMaterial.hpp"

#include "yqlora/core/EngineServices.hpp"
#include "yqlora/material/MaterialData.hpp"
#include "yqlora/resource/ResourceCache.hpp"

#include <string>

namespace yqlora {

SharedMaterial::SharedMaterial(const EngineServices& services, ResourceRef material)
    : m_services(services)
    , m_material(material)
{
}

bool SharedMaterial::change(const std::function<void(MaterialData&)>& edit)
{
    ResourceCache& cache = m_services.resources();
    const MaterialData* current = cache.find<MaterialData>(m_material.id);
    if (!current) {
        return false;
    }
    MaterialData changed = *current;
    edit(changed);

    return cache.replace(m_material.id, std::move(changed));
}

bool SharedMaterial::setNumbers(std::string_view name, std::vector<float> numbers)
{
    return change([&](MaterialData& material) {
        material.values[std::string(name)] = std::move(numbers);
    });
}

bool SharedMaterial::setFloat(std::string_view name, float value)
{
    return setNumbers(name, {value});
}

bool SharedMaterial::setVector(std::string_view name, const Vec2& value)
{
    return setNumbers(name, {value.x, value.y});
}

bool SharedMaterial::setVector(std::string_view name, const Vec3& value)
{
    return setNumbers(name, {value.x, value.y, value.z});
}

bool SharedMaterial::setVector(std::string_view name, const Vec4& value)
{
    return setNumbers(name, {value.x, value.y, value.z, value.w});
}

bool SharedMaterial::setColor(std::string_view name, const Vec4& value)
{
    return setVector(name, value);
}

bool SharedMaterial::setTexture(std::string_view name, ResourceRef texture)
{
    return change([&](MaterialData& material) {
        material.textures[std::string(name)] = texture.id;
    });
}

} // namespace yqlora
