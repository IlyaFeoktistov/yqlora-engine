#include "yqlora/render/PipelineCache.hpp"

#include "yqlora/core/Log.hpp"
#include "yqlora/render/GpuMesh.hpp"

#include <iterator>
#include <map>
#include <stdexcept>

namespace yqlora {

PipelineCache::PipelineCache(rhi::Device& device)
    : m_device(device)
{
}

PipelineCache::~PipelineCache()
{
    // Конвейеры ссылаются на шейдеры: уходят первыми.
    m_pipelines.clear();
}

const rhi::Pipeline* PipelineCache::get(const FrameMaterial& material, rhi::Format color, rhi::Format depth)
{
    std::unique_ptr<rhi::Shader>& shader = m_shaders.get(material.shaderId, material.shaderRevision, [&] {
        // Прежняя ревизия шейдера сейчас уйдёт: её конвейеры - раньше неё.
        std::erase_if(m_pipelines, [&](const auto& pipeline) {
            return std::get<0>(pipeline.first) == material.shaderId;
        });
        auto created = std::make_unique<std::unique_ptr<rhi::Shader>>();
        try {
            *created = m_device.createShader(material.shader->desc);
        } catch (const std::runtime_error& error) {
            log::error(
                "Shader '{}' is not built by the graphics backend: {}",
                material.shader->desc.name,
                error.what()
            );
        }

        return created;
    });
    if (!shader) {
        return nullptr;
    }
    const Key key{material.shaderId, material.shaderRevision, material.data->blend, material.data->cull, color, depth};
    Entry& entry = m_pipelines[key];
    if (!entry.pipeline) {
        const bool transparent = material.data->blend == MaterialBlend::Transparent;
        entry.pipeline = m_device.createPipeline({
            .shader = shader.get(),
            .vertexLayout = meshVertexLayout(),
            .depthTest = true,
            .depthWrite = !transparent,
            .cull = material.data->cull == MaterialCull::Back ? rhi::CullMode::Back : rhi::CullMode::None,
            .blend = transparent ? rhi::BlendMode::Alpha : rhi::BlendMode::Opaque,
            .colorFormat = color,
            .depthFormat = depth,
        });
    }
    entry.lastUsedFrame = m_frame;

    return entry.pipeline.get();
}

void PipelineCache::endFrame()
{
    ++m_frame;
    for (auto it = m_pipelines.begin(); it != m_pipelines.end();) {
        const bool unused = m_frame - it->second.lastUsedFrame > GpuResourceCache<rhi::Shader>::kUnusedFramesToKeep;
        it = unused ? m_pipelines.erase(it) : std::next(it);
    }
    m_shaders.endFrame();
}

} // namespace yqlora
