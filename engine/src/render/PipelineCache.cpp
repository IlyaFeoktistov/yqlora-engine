#include "yqlora/render/PipelineCache.hpp"

#include "yqlora/core/Log.hpp"
#include "yqlora/render/GpuMesh.hpp"
#include "yqlora/render/ShadowPass.hpp"

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

namespace {

// Состояние конвейера по проходу и материалу.
rhi::PipelineDesc pipelineDesc(const FrameMaterial& material, MaterialPass pass)
{
    if (pass == MaterialPass::Capture) {
        // В грань куба целиком, без глубины.
        return {.depthTest = false, .depthWrite = false, .cull = rhi::CullMode::None};
    }
    if (pass == MaterialPass::ShadowDepth) {
        // Как общий шейдер глубины ShadowPass: тень отбрасывают и тонкие
        // поверхности, видимые с одной стороны.
        return {
            .vertexLayout = meshVertexLayout(),
            .depthTest = true,
            .depthWrite = true,
            .depthBias = ShadowPass::kDepthBias,
            .cull = rhi::CullMode::None,
        };
    }
    if (pass == MaterialPass::Sky) {
        // Небо на глубине 1: проходит там, где после очистки ничего не нарисовано.
        return {
            .depthTest = true,
            .depthWrite = false,
            .depthCompare = rhi::CompareOp::LessEqual,
            .cull = rhi::CullMode::None,
        };
    }
    const bool transparent = material.data->blend == MaterialBlend::Transparent;

    return {
        .vertexLayout = meshVertexLayout(),
        .depthTest = true,
        .depthWrite = !transparent,
        .cull = material.data->cull == MaterialCull::Back ? rhi::CullMode::Back : rhi::CullMode::None,
        .blend = transparent ? rhi::BlendMode::Alpha : rhi::BlendMode::Opaque,
    };
}

// Шейдер для бэкенда; nullptr - не собрался (сообщение в логе).
std::unique_ptr<rhi::Shader> createShader(rhi::Device& device, const rhi::ShaderDesc& desc)
{
    try {
        return device.createShader(desc);
    } catch (const std::runtime_error& error) {
        log::error("Shader '{}' is not built by the graphics backend: {}", desc.name, error.what());

        return nullptr;
    }
}

} // namespace

const rhi::Pipeline* PipelineCache::get(
    const FrameMaterial& material,
    MaterialPass pass,
    rhi::Format color,
    rhi::Format depth
)
{
    Shaders& shaders = m_shaders.get(material.shaderId, material.shaderRevision, [&] {
        // Прежняя ревизия шейдера сейчас уйдёт: её конвейеры - раньше неё.
        std::erase_if(m_pipelines, [&](const auto& pipeline) {
            return std::get<0>(pipeline.first) == material.shaderId;
        });
        auto created = std::make_unique<Shaders>();
        created->main = createShader(m_device, material.shader->desc);
        if (material.shader->captureDesc) {
            created->capture = createShader(m_device, *material.shader->captureDesc);
        }
        if (material.shader->depthDesc) {
            created->depth = createShader(m_device, *material.shader->depthDesc);
        }

        return created;
    });
    const rhi::Shader* shader = nullptr;
    switch (pass) {
    case MaterialPass::Surface:
    case MaterialPass::Sky: shader = shaders.main.get(); break;
    case MaterialPass::Capture: shader = shaders.capture.get(); break;
    case MaterialPass::ShadowDepth: shader = shaders.depth.get(); break;
    }
    if (!shader) {
        return nullptr;
    }
    const Key key{
        material.shaderId,
        material.shaderRevision,
        pass,
        material.data->blend,
        material.data->cull,
        color,
        depth,
    };
    Entry& entry = m_pipelines[key];
    if (!entry.pipeline) {
        rhi::PipelineDesc desc = pipelineDesc(material, pass);
        desc.shader = shader;
        desc.colorFormat = color;
        desc.depthFormat = depth;
        entry.pipeline = m_device.createPipeline(std::move(desc));
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
