#include "yqlora/material/MaterialOverrides.hpp"

namespace yqlora {

void MaterialOverrides::set(std::string_view name, MaterialOverrideValue value)
{
    m_values.insert_or_assign(std::string(name), std::move(value));
}

void MaterialOverrides::setFloat(std::string_view name, float value)
{
    set(name, {{value}});
}

void MaterialOverrides::setVector(std::string_view name, const Vec2& value)
{
    set(name, {{value.x, value.y}});
}

void MaterialOverrides::setVector(std::string_view name, const Vec3& value)
{
    set(name, {{value.x, value.y, value.z}});
}

void MaterialOverrides::setVector(std::string_view name, const Vec4& value)
{
    set(name, {{value.x, value.y, value.z, value.w}});
}

void MaterialOverrides::setColor(std::string_view name, const Vec3& value)
{
    set(name, {{value.x, value.y, value.z}, true});
}

void MaterialOverrides::setColor(std::string_view name, const Vec4& value)
{
    set(name, {{value.x, value.y, value.z, value.w}, true});
}

void MaterialOverrides::setTexture(std::string_view name, ResourceRef texture)
{
    m_textures.insert_or_assign(std::string(name), texture);
}

void MaterialOverrides::clear(std::string_view name)
{
    if (const auto value = m_values.find(name); value != m_values.end()) {
        m_values.erase(value);
    }
    if (const auto texture = m_textures.find(name); texture != m_textures.end()) {
        m_textures.erase(texture);
    }
}

void MaterialOverrides::clearAll()
{
    m_values.clear();
    m_textures.clear();
}

} // namespace yqlora
