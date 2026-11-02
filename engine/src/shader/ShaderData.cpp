#include "yqlora/shader/ShaderData.hpp"

#include <algorithm>

namespace yqlora {

const MaterialField* ShaderData::findField(std::string_view name) const
{
    const auto it = std::ranges::find(fields, name, &MaterialField::name);

    return it != fields.end() ? &*it : nullptr;
}

const MaterialTexture* ShaderData::findTexture(std::string_view name) const
{
    const auto it = std::ranges::find(textures, name, &MaterialTexture::name);

    return it != textures.end() ? &*it : nullptr;
}

} // namespace yqlora
