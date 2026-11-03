#include "yqlora/material/MaterialData.hpp"

#include "yqlora/shader/ShaderData.hpp"

namespace yqlora {

std::span<const float> materialValue(const MaterialData& material, const MaterialField& field)
{
    const auto found = material.values.find(field.name);
    // Значение другого типа (поле стало float, а хранится vec4) не берётся,
    // но остаётся в материале.
    if (found != material.values.end() && found->second.size() == field.defaultValue.size()) {
        return found->second;
    }

    return field.defaultValue;
}

ResourceId materialTexture(const MaterialData& material, const MaterialTexture& texture)
{
    const auto found = material.textures.find(texture.name);

    return found != material.textures.end() ? found->second : 0;
}

} // namespace yqlora
