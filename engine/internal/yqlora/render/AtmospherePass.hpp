#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/render/EngineBindings.hpp"
#include "yqlora/render/EngineUniforms.hpp"
#include "yqlora/render/RenderGraph.hpp"
#include "yqlora/rhi/Device.hpp"
#include "yqlora/shader/ShaderCompiler.hpp"

#include <memory>
#include <optional>
#include <vector>

namespace yqlora {

struct RenderContext;

// Таблицы атмосферы (atmosphere.glsl) в графе кадра. Пропускание и
// многократное рассеяние зависят только от атмосферы: они живут между
// кадрами и заполняются заново, когда она меняется. Вид неба и воздушная
// перспектива зависят от камеры и солнца: они временные, на каждый вид.
// Без атмосферы проходов нет, а шейдеры получают пустые таблицы.
class YQLORA_API AtmospherePass {
public:
    static constexpr int kTransmittanceWidth = 256;
    static constexpr int kTransmittanceHeight = 64;
    static constexpr int kMultipleScatteringSize = 32;
    static constexpr int kSkyViewWidth = 192;
    static constexpr int kSkyViewHeight = 108;
    // Срезы воздушной перспективы, каждый kAerialSliceSize x kAerialSliceSize.
    static constexpr int kAerialSlices = 32;
    static constexpr int kAerialSliceSize = 32;

    // Таблицы кадра в графе: каждая текстура импортирована один раз, чтобы
    // граф видел, кто её пишет и кто читает.
    struct FrameLuts {
        RenderGraph::TextureId transmittance = 0;
        RenderGraph::TextureId multipleScattering = 0;
        RenderGraph::TextureId empty = 0;
    };

    struct ViewLuts {
        RenderGraph::TextureId skyView = 0;
        RenderGraph::TextureId aerialPerspective = 0;
    };

    explicit AtmospherePass(const RenderContext& context);
    ~AtmospherePass();

    AtmospherePass(const AtmospherePass&) = delete;
    AtmospherePass& operator=(const AtmospherePass&) = delete;

    // Таблицы кадра; их проходы - если атмосфера изменилась. resources -
    // блоки кадра и атмосферы.
    FrameLuts addFrame(RenderGraph& graph, const AtmosphereUniforms& atmosphere, const EngineResources& resources);

    // Таблицы вида; resources - блоки кадра, атмосферы и этого вида.
    ViewLuts addView(
        RenderGraph& graph,
        const AtmosphereUniforms& atmosphere,
        const EngineResources& resources,
        const FrameLuts& frame
    );

    const rhi::Sampler& lutSampler() const
    {
        return *m_sampler;
    }

private:
    struct Lut {
        std::unique_ptr<rhi::Shader> shader;
        std::unique_ptr<rhi::Pipeline> pipeline;
    };

    Lut makeLut(const char* file, std::vector<ShaderDefine> defines = {});
    // Проход, который рисует таблицу target шейдером lut, читая reads.
    void addLutPass(
        RenderGraph& graph,
        const char* name,
        const Lut& lut,
        RenderGraph::TextureId target,
        std::vector<RenderGraph::TextureId> reads,
        const EngineResources& resources,
        std::optional<RenderGraph::TextureId> transmittance,
        std::optional<RenderGraph::TextureId> multipleScattering
    );

    const RenderContext& m_context;
    std::unique_ptr<rhi::Sampler> m_sampler;
    Lut m_transmittanceLut;
    Lut m_multipleScatteringLut;
    Lut m_skyViewLut;
    Lut m_aerialPerspectiveLut;
    std::unique_ptr<rhi::Texture> m_transmittance;
    std::unique_ptr<rhi::Texture> m_multipleScattering;
    // Таблица вместо таблиц вида, когда атмосферы нет: читается, но не влияет.
    std::unique_ptr<rhi::Texture> m_empty;
    // Атмосфера, для которой заполнены таблицы на кадр; nullopt - ещё не заполнены.
    std::optional<AtmosphereUniforms> m_filledFor;
};

} // namespace yqlora
