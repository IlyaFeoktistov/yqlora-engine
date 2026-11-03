#pragma once

#include "yqlora/render/FrameData.hpp"
#include "yqlora/render/UniformRing.hpp"
#include "yqlora/render/ViewPass.hpp"
#include "yqlora/rhi/Device.hpp"

#include <memory>
#include <set>
#include <span>
#include <string>
#include <utility>

namespace yqlora {

// Подмеши вида их материалами, в HDR-цель кадра.
class MeshPass {
public:
    explicit MeshPass(const RenderContext& context);
    ~MeshPass();

    // Подмеши в открытый проход, в порядке visible (Prepare).
    void draw(
        rhi::CommandList& commands,
        const FrameData& frame,
        std::span<const DrawItem* const> visible,
        const UniformSlice& frameUniforms,
        const UniformSlice& viewUniforms
    );

private:
    const RenderContext& m_context;
    // Материал Error: им рисуется материал, чей шейдер бэкенд не собрал.
    std::shared_ptr<const void> m_errorShaderData;
    std::shared_ptr<const void> m_errorMaterialData;
    FrameMaterial m_error;
    // Замены, о которых уже сказано в логе: шейдер и имя поля.
    std::set<std::pair<ResourceId, std::string>> m_reportedOverrides;
};

} // namespace yqlora
