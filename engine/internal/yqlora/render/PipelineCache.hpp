#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/render/FrameData.hpp"
#include "yqlora/render/GpuResourceCache.hpp"
#include "yqlora/rhi/Device.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <tuple>
#include <utility>

namespace yqlora {

// Чем рисует материал: подмеш (вершины меша, смешивание и отсечение из
// материала), небо (треугольник на весь экран на дальней плоскости) или
// небо в грань куба для освещения (вариант захвата шейдера неба).
enum class MaterialPass : std::uint8_t {
    Surface,
    Sky,
    Capture,
};

// Конвейеры материалов: по шейдеру и его ревизии, смешиванию, отсечению и
// форматам цели - один конвейер на состояние. Шейдер в видеопамяти - один на
// ревизию; новая ревизия даёт новые конвейеры, старые освобождаются по
// правилу GpuResourceCache.
class YQLORA_API PipelineCache {
public:
    explicit PipelineCache(rhi::Device& device);
    ~PipelineCache();

    // nullptr, если бэкенд не собрал шейдер (сообщение в логе один раз на
    // ревизию): тогда рисуется материал Error.
    const rhi::Pipeline* get(const FrameMaterial& material, MaterialPass pass, rhi::Format color, rhi::Format depth);

    void endFrame();

    std::size_t size() const
    {
        return m_pipelines.size();
    }

private:
    using Key = std::
        tuple<ResourceId, std::uint64_t, MaterialPass, MaterialBlend, MaterialCull, rhi::Format, rhi::Format>;

    struct Entry {
        std::unique_ptr<rhi::Pipeline> pipeline;
        std::uint64_t lastUsedFrame = 0;
    };

    rhi::Device& m_device;
    // Шейдер ревизии в видеопамяти с вариантом захвата, если он есть. Вариант,
    // который бэкенд не собрал, - nullptr.
    struct Shaders {
        std::unique_ptr<rhi::Shader> main;
        std::unique_ptr<rhi::Shader> capture;
    };

    GpuResourceCache<Shaders> m_shaders;
    std::map<Key, Entry> m_pipelines;
    std::uint64_t m_frame = 0;
};

} // namespace yqlora
