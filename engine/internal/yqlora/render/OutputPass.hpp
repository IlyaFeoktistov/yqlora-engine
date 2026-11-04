#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/render/RenderGraph.hpp"
#include "yqlora/render/ToneMapping.hpp"
#include "yqlora/rhi/Device.hpp"

#include <memory>
#include <vector>

namespace yqlora {

struct RenderContext;

// Проход вывода: линейная HDR-цель кадра (уже с экспозицией) через тональную
// компрессию в sRGB итоговой цели (окно или RenderTarget, kOutputColorFormat),
// которую интерфейс показывает как есть. На каждую компрессию - свой вариант
// шейдера: выбор не стоит ветвления в каждом пикселе.
class YQLORA_API OutputPass {
public:
    explicit OutputPass(const RenderContext& context);
    ~OutputPass();

    void addTo(
        RenderGraph& graph,
        RenderGraph::TextureId hdr,
        RenderGraph::TextureId target,
        ToneMapping toneMapping
    ) const;

private:
    struct Variant {
        std::unique_ptr<rhi::Shader> shader;
        std::unique_ptr<rhi::Pipeline> pipeline;
    };

    // По значениям ToneMapping.
    std::vector<Variant> m_variants;
    std::unique_ptr<rhi::Sampler> m_sampler;
};

} // namespace yqlora
