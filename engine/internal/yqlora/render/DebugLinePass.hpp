#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/render/BufferRing.hpp"
#include "yqlora/render/FrameData.hpp"
#include "yqlora/render/UniformRing.hpp"
#include "yqlora/rhi/Device.hpp"

#include <cstdint>
#include <memory>

namespace yqlora {

struct RenderContext;

// Отладочные линии кадра (FrameData::debugLines) отрезками в пиксель:
// сначала с тестом глубины, затем поверх всего. Рисуются внутри прохода
// каждого вида после прозрачного, в его HDR-цель, глубину не пишут.
class YQLORA_API DebugLinePass {
public:
    // Вершины кадра в буфере кадра: одни на все виды.
    struct Lines {
        BufferSlice tested;
        std::uint32_t testedCount = 0;
        BufferSlice overlay;
        std::uint32_t overlayCount = 0;
    };

    explicit DebugLinePass(const RenderContext& context);
    ~DebugLinePass();

    Lines upload(const FrameData& frame) const;

    // В открытом проходе вида; view - его блок View.
    void draw(rhi::CommandList& commands, const Lines& lines, const UniformSlice& view) const;

private:
    BufferRing& m_vertices;
    std::unique_ptr<rhi::Shader> m_shader;
    std::unique_ptr<rhi::Pipeline> m_tested;
    std::unique_ptr<rhi::Pipeline> m_overlay;
};

} // namespace yqlora
