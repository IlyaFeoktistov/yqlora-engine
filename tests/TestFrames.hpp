#pragma once

#include <yqlora/material/MaterialData.hpp>
#include <yqlora/render/FrameData.hpp>
#include <yqlora/resource/BuiltinResources.hpp>
#include <yqlora/resource/ResourceCache.hpp>
#include <yqlora/shader/ShaderData.hpp>

#include <cstddef>
#include <deque>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace yqlora::test {

// Материалы кадров, собранных в тесте вручную: данные живут, пока жив объект.
class TestMaterials {
public:
    // Материал Standard с базовым цветом color (sRGB) и значениями values
    // поверх; его индекс в frame.materials.
    std::size_t add(
        FrameData& frame,
        const Vec4& color = Vec4(1.0f),
        MaterialBlend blend = MaterialBlend::Opaque,
        const std::map<std::string, std::vector<float>>& values = {},
        MaterialCull cull = MaterialCull::Back
    )
    {
        MaterialData& material = m_data.emplace_back(
            MaterialData{
                .blend = blend,
                .cull = cull,
                .values = {{"baseColor", {color.r, color.g, color.b, color.a}}},
            }
        );
        material.values.insert(values.begin(), values.end());
        const LoadedData<ShaderData> shader = m_cache.get<ShaderData>(builtin::kStandardShader);
        FrameMaterial frameMaterial{
            .id = nextId(),
            .revision = 1,
            .data = &material,
            .shaderId = builtin::kStandardShader,
            .shaderRevision = shader.revision,
            .shader = shader.data,
        };
        // Карт нет: у каждой текстуры шейдера - текстура по умолчанию.
        for (const MaterialTexture& texture : shader.data->textures) {
            frameMaterial.textures.push_back({.slot = texture.slot, .fallback = texture.defaultTexture});
        }
        frame.materials.push_back(std::move(frameMaterial));

        return frame.materials.size() - 1;
    }

private:
    // ID материалов тестов: вне встроенных и разные на весь прогон, ведь
    // рендерер может пережить TestMaterials, а его кэш помнит материал по ID.
    static ResourceId nextId()
    {
        static ResourceId next = builtin::kFirstProjectId;

        return next++;
    }

    ResourceCache m_cache;
    std::deque<MaterialData> m_data;
};

} // namespace yqlora::test
