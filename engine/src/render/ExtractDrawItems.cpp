#include "yqlora/render/FrameExtract.hpp"

#include "yqlora/core/EngineServices.hpp"
#include "yqlora/render/MeshRenderer.hpp"
#include "yqlora/render/Prepare.hpp"
#include "yqlora/render/TextureData.hpp"
#include "yqlora/resource/ResourceCache.hpp"
#include "yqlora/scene/Scene.hpp"

#include <map>

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

// Материалы кадра: каждый находится в кэше один раз за кадр.
class FrameMaterials {
public:
    FrameMaterials(ResourceCache& cache, FrameData& frame)
        : m_cache(cache)
        , m_frame(frame)
    {
    }

    // Индекс материала слота в FrameData::materials; пустая ссылка - материал
    // по умолчанию.
    std::size_t index(ResourceId id)
    {
        const ResourceId wanted = id != 0 ? id : builtin::kDefaultMaterial;
        if (const auto found = m_indices.find(wanted); found != m_indices.end()) {
            return found->second;
        }
        m_frame.materials.push_back(resolve(wanted));

        return m_indices[wanted] = m_frame.materials.size() - 1;
    }

    // Замены слота поверх материала index: копия значений и текстуры.
    std::size_t addOverrides(std::size_t index, const MaterialOverrides& overrides)
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

private:
    // Материал без данных (пропал, не прочитался) или без шейдера рисуется
    // материалом Error, чтобы ошибку было видно.
    FrameMaterial resolve(ResourceId id)
    {
        LoadedData<MaterialData> material = m_cache.get<MaterialData>(id);
        LoadedData<ShaderData> shader = material ? m_cache.get<ShaderData>(material.data->shader)
                                                 : LoadedData<ShaderData>{};
        if (!material || !shader) {
            id = builtin::kErrorMaterial;
            material = m_cache.get<MaterialData>(id);
            shader = m_cache.get<ShaderData>(material.data->shader);
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

    ResourceCache& m_cache;
    FrameData& m_frame;
    std::map<ResourceId, std::size_t> m_indices;
};

} // namespace

void extractDrawItems(const Scene& scene, FrameData& frame)
{
    ResourceCache& resources = scene.services().resources();
    FrameMaterials materials(resources, frame);
    for (const MeshRenderer* renderer : scene.components<MeshRenderer>()) {
        if (!renderer->activeAndEnabled()) {
            continue;
        }
        const auto* node = dynamic_cast<const Node3D*>(renderer->owner());
        const ResourceId meshId = renderer->mesh().id;
        const LoadedData<MeshData> mesh = resources.get<MeshData>(meshId);
        if (!node || !mesh) {
            continue;
        }
        const Bounds bounds = transformBounds(mesh.data->bounds, node->worldMatrix());
        for (std::size_t slot = 0; slot < mesh.data->submeshes.size(); ++slot) {
            const SubMesh& submesh = mesh.data->submeshes[slot];
            DrawItem item{
                .mesh = mesh.data,
                .meshId = meshId,
                .meshRevision = mesh.revision,
                .firstIndex = submesh.firstIndex,
                .indexCount = submesh.indexCount,
                .world = node->worldMatrix(),
                .bounds = bounds,
                .material = materials.index(renderer->material(slot).id),
                .layer = node->layer(),
                .node = node->id(),
            };
            if (const MaterialOverrides* overrides = renderer->findOverrides(slot)) {
                item.overrides = materials.addOverrides(item.material, *overrides);
            }
            frame.drawItems.push_back(item);
        }
    }
}

} // namespace yqlora
