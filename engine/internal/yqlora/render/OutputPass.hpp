#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/render/RenderGraph.hpp"
#include "yqlora/rhi/Device.hpp"

#include <memory>

namespace yqlora {

struct RenderContext;

// Проход вывода: линейная HDR-цель кадра в sRGB итоговой цели (окно или
// RenderTarget, kOutputColorFormat), которую интерфейс показывает как есть. Позже в нём
// же тональная компрессия и экспозиция.
class YQLORA_API OutputPass {
public:
    explicit OutputPass(const RenderContext& context);

    void addTo(RenderGraph& graph, RenderGraph::TextureId hdr, RenderGraph::TextureId target) const;

private:
    std::unique_ptr<rhi::Shader> m_shader;
    std::unique_ptr<rhi::Pipeline> m_pipeline;
    std::unique_ptr<rhi::Sampler> m_sampler;
};

} // namespace yqlora
