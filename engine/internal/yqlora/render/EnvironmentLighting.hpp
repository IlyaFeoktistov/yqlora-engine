#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/render/EngineBindings.hpp"
#include "yqlora/render/EngineUniforms.hpp"
#include "yqlora/render/FrameData.hpp"
#include "yqlora/render/RenderGraph.hpp"
#include "yqlora/rhi/Device.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace yqlora {

struct RenderContext;

// Свет неба для освещения (lighting.glsl): небо окружения рисуется в куб
// (вариант захвата его шейдера), из куба - 9 коэффициентов гармоник для
// рассеянного света и куб отражений с мипами по шероховатости; таблица
// BRDF строится один раз. Всё это живёт между кадрами и строится заново,
// только когда небо меняется (материал, его значения и текстуры, солнце,
// атмосфера, высота камеры шагами), так что смена дня и ночи стоит один
// захват на изменение, а без изменений - ничего. Без света неба таблицы
// чёрные.
class YQLORA_API EnvironmentLighting {
public:
    static constexpr int kCaptureSize = 128;
    // Мипы куба отражений от 128 до 4: шероховатость 0..1 по ним.
    static constexpr int kSpecularMips = 6;
    static constexpr float kMaxSpecularMip = static_cast<float>(kSpecularMips - 1);
    static constexpr int kBrdfSize = 128;
    // Камера поднимается выше этого шага - небо захватывается заново.
    static constexpr float kHeightStep = 100.0f;

    struct Textures {
        RenderGraph::TextureId harmonics = 0;
        RenderGraph::TextureId specular = 0;
        RenderGraph::TextureId brdf = 0;
    };

    explicit EnvironmentLighting(const RenderContext& context);
    ~EnvironmentLighting();

    EnvironmentLighting(const EnvironmentLighting&) = delete;
    EnvironmentLighting& operator=(const EnvironmentLighting&) = delete;

    // Таблицы света неба кадра; их проходы - если небо изменилось. resources
    // и atmosphere - ресурсы движка и таблицы атмосферы первого вида: небо
    // захватывается из его камеры.
    Textures addTo(
        RenderGraph& graph,
        const FrameData& frame,
        const EngineResources& resources,
        const EngineTextureIds& atmosphere
    );

    const rhi::Sampler& sampler() const
    {
        return *m_sampler;
    }

    // Сколько раз небо захватывалось: для проверки, что без изменений захвата нет.
    std::size_t captures() const
    {
        return m_captures;
    }

private:
    struct Pass {
        std::unique_ptr<rhi::Shader> shader;
        std::unique_ptr<rhi::Pipeline> pipeline;
    };

    Pass makePass(const char* file);
    // То, от чего зависит свет неба.
    struct Key {
        ResourceId material = 0;
        ResourceId shader = 0;
        std::uint64_t shaderRevision = 0;
        // Блок Material с заменами: значения неба.
        std::vector<std::byte> block{};
        // ID и ревизии текстур неба.
        std::vector<std::pair<ResourceId, std::uint64_t>> textures{};
        int hasSun = 0;
        Vec3 sunDirection{0.0f};
        Vec3 sunColor{0.0f};
        AtmosphereUniforms atmosphere{};
        // Высота камеры шагами kHeightStep.
        int heightStep = 0;

        bool operator==(const Key&) const = default;
    };

    // nullopt - света неба нет.
    static std::optional<Key> keyOf(const FrameData& frame);
    void addCapture(
        RenderGraph& graph,
        const FrameData& frame,
        const EngineResources& resources,
        const EngineTextureIds& atmosphere,
        const Textures& textures,
        RenderGraph::TextureId capture
    );
    void addClear(RenderGraph& graph, const Textures& textures);

    const RenderContext& m_context;
    std::unique_ptr<rhi::Sampler> m_sampler;
    Pass m_harmonicsPass;
    Pass m_prefilterPass;
    Pass m_brdfPass;
    std::unique_ptr<rhi::Texture> m_capture;
    std::unique_ptr<rhi::Texture> m_specular;
    std::unique_ptr<rhi::Texture> m_harmonics;
    std::unique_ptr<rhi::Texture> m_brdf;
    bool m_brdfReady = false;
    // Для чего построены таблицы: внешний nullopt - ещё не построены,
    // внутренний - построены без света неба.
    std::optional<std::optional<Key>> m_builtFor;
    std::size_t m_captures = 0;
};

} // namespace yqlora
