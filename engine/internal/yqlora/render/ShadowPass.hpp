#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/render/FrameData.hpp"
#include "yqlora/render/RenderGraph.hpp"
#include "yqlora/render/ShadowCascades.hpp"
#include "yqlora/rhi/Device.hpp"

#include <memory>
#include <vector>

namespace yqlora {

struct RenderContext;

// Карты теней кадра в графе (shadows.glsl читает их): атлас точечных и
// прожекторов - один на кадр, карта каскадов солнца - на вид, потому что
// каскады следуют за его камерой. Проходы - только глубина, до вида, по
// области вывода на каскад и плитку; рисуются непрозрачные меши с
// castShadows, каждый лишь в те карты, чью пирамиду задевает. Без теней
// карта - 1 x 1, стёртая в "свет доходит": шейдеры освещения всегда
// получают обе.
class YQLORA_API ShadowPass {
public:
    explicit ShadowPass(const RenderContext& context);
    ~ShadowPass();

    ShadowPass(const ShadowPass&) = delete;
    ShadowPass& operator=(const ShadowPass&) = delete;

    // Атлас плиток FrameData::shadowTiles.
    RenderGraph::TextureId addAtlas(RenderGraph& graph, const FrameData& frame);

    // Каскады солнца для камеры вида; cascades получает их (пусто - солнце
    // без теней).
    RenderGraph::TextureId addCascades(
        RenderGraph& graph,
        const FrameData& frame,
        const ViewData& view,
        std::vector<ShadowCascade>& cascades
    );

    // Сравнение глубины с линейным фильтром (shadowCascades, shadowAtlas).
    const rhi::Sampler& compareSampler() const
    {
        return *m_compareSampler;
    }

    // Глубина как есть, ближайший тексель (поиск заслоняющих).
    const rhi::Sampler& depthSampler() const
    {
        return *m_depthSampler;
    }

private:
    struct Region {
        Mat4 viewProjection;
        rhi::Viewport viewport;
    };

    RenderGraph::TextureId addMap(
        RenderGraph& graph,
        const FrameData& frame,
        const char* name,
        int size,
        std::vector<Region> regions
    );

    const RenderContext& m_context;
    std::unique_ptr<rhi::Shader> m_shader;
    std::unique_ptr<rhi::Pipeline> m_pipeline;
    std::unique_ptr<rhi::Sampler> m_compareSampler;
    std::unique_ptr<rhi::Sampler> m_depthSampler;
};

} // namespace yqlora
