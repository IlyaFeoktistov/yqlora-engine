#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/render/FrameData.hpp"

#include <cstddef>
#include <map>
#include <optional>
#include <utility>

namespace yqlora {

class ResourceCache;

// Материалы кадра (FrameData::materials): каждый находится в кэше ресурсов
// один раз за кадр, с шейдером и текстурами. Материал проверяется по тому,
// где он стоит: в слоте меша нужен шейдер поверхности, в небе - шейдер неба.
class YQLORA_API FrameMaterials {
public:
    FrameMaterials(ResourceCache& cache, FrameData& frame);

    // Индекс материала слота меша; пустая ссылка - материал по умолчанию.
    // Материал без данных, без шейдера или с шейдером неба рисуется
    // материалом Error, чтобы ошибку было видно.
    std::size_t surface(ResourceId id);

    // Индекс материала неба; nullopt - ссылка пустая, материала нет или его
    // шейдер не неба (тогда *misfit = true, чтобы вызвавший сказал об этом).
    std::optional<std::size_t> sky(ResourceId id, bool* misfit = nullptr);

    // Замены слота поверх материала index: копия значений и текстуры; индекс
    // в FrameData::overrides.
    std::size_t addOverrides(std::size_t index, const MaterialOverrides& overrides);

private:
    // Материал со своим шейдером и текстурами; nullopt - нет данных или шейдера.
    std::optional<FrameMaterial> resolve(ResourceId id);
    std::size_t add(ResourceId id, ShaderKind use, FrameMaterial material);

    ResourceCache& m_cache;
    FrameData& m_frame;
    std::map<std::pair<ResourceId, ShaderKind>, std::size_t> m_indices;
};

} // namespace yqlora
