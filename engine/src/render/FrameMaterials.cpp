#include "yqlora/render/FrameMaterials.hpp"

#include "yqlora/render/TextureData.hpp"
#include "yqlora/resource/ResourceCache.hpp"

namespace yqlora {

namespace {

FrameTexture frameTexture(ResourceCache& cache, const MaterialTexture& texture, ResourceId id)
{
    FrameTexture result{.slot = texture.slot, .id = id, .fallback = texture.defaultTexture};
    if (id != 0) {
        const LoadedData<TextureData> loaded = cache.get<TextureData>(id);
        result.data = loaded.data;
        result.revision = loaded.revision;
    }

    return result;
}

} // namespace

FrameMaterials::FrameMaterials(ResourceCache& cache, FrameData& frame)
    : m_cache(cache)
    , m_frame(frame)
{
}

std::size_t FrameMaterials::surface(ResourceId id)
{
    const ResourceId wanted = id != 0 ? id : builtin::kDefaultMaterial;
    if (const auto found = m_indices.find({wanted, ShaderKind::Surface}); found != m_indices.end()) {
        return found->second;
    }
    std::optional<FrameMaterial> material = resolve(wanted);
    if (!material || material->shader->kind != ShaderKind::Surface) {
        material = resolve(builtin::kErrorMaterial);
    }

    return add(wanted, ShaderKind::Surface, std::move(*material));
}

std::optional<std::size_t> FrameMaterials::sky(ResourceId id, bool* misfit)
{
    if (id == 0) {
        return std::nullopt;
    }
    if (const auto found = m_indices.find({id, ShaderKind::Sky}); found != m_indices.end()) {
        return found->second;
    }
    std::optional<FrameMaterial> material = resolve(id);
    if (!material) {
        return std::nullopt;
    }
    if (material->shader->kind != ShaderKind::Sky) {
        if (misfit) {
            *misfit = true;
        }

        return std::nullopt;
    }

    return add(id, ShaderKind::Sky, std::move(*material));
}

std::size_t FrameMaterials::addOverrides(std::size_t index, const MaterialOverrides& overrides)
{
    const FrameMaterial& material = m_frame.materials[index];
    FrameOverrides frame{.values = overrides, .textures = material.textures};
    for (std::size_t i = 0; i < material.shader->textures.size(); ++i) {
        const MaterialTexture& texture = material.shader->textures[i];
        if (const auto replaced = overrides.textures().find(texture.name); replaced != overrides.textures().end()) {
            frame.textures[i] = frameTexture(m_cache, texture, replaced->second.id);
        }
    }
    m_frame.overrides.push_back(std::move(frame));

    return m_frame.overrides.size() - 1;
}

std::optional<FrameMaterial> FrameMaterials::resolve(ResourceId id)
{
    const LoadedData<MaterialData> material = m_cache.get<MaterialData>(id);
    if (!material) {
        return std::nullopt;
    }
    const LoadedData<ShaderData> shader = m_cache.get<ShaderData>(material.data->shader);
    if (!shader) {
        return std::nullopt;
    }
    FrameMaterial result{
        .id = id,
        .revision = material.revision,
        .data = material.data,
        .shaderId = material.data->shader,
        .shaderRevision = shader.revision,
        .shader = shader.data,
    };
    for (const MaterialTexture& texture : shader.data->textures) {
        result.textures.push_back(frameTexture(m_cache, texture, materialTexture(*material.data, texture)));
    }

    return result;
}

std::size_t FrameMaterials::add(ResourceId id, ShaderKind use, FrameMaterial material)
{
    m_frame.materials.push_back(std::move(material));

    return m_indices[{id, use}] = m_frame.materials.size() - 1;
}

} // namespace yqlora
