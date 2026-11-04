#pragma once

#include "yqlora/math/Math.hpp"
#include "yqlora/render/ViewPass.hpp"

#include <memory>

namespace yqlora::editor {

// Фон окна сцены редактора: цвет темы там, где сцена ничего не нарисовала.
// Рисуется поверх итоговой цели после прохода вывода, поэтому цвет выходит
// ровно таким, как в теме, без экспозиции и тональной компрессии кадра.
class BackgroundPass final : public ViewPass {
public:
    explicit BackgroundPass(const RenderContext& context);

    // Цвет в sRGB.
    void setColor(const Vec4& color)
    {
        m_color = color;
    }

    void addTo(RenderGraph& graph, const ViewPassInput& input) override;

private:
    UniformRing& m_uniforms;
    std::unique_ptr<rhi::Shader> m_shader;
    std::unique_ptr<rhi::Pipeline> m_pipeline;
    Vec4 m_color{0.0f, 0.0f, 0.0f, 1.0f};
};

} // namespace yqlora::editor
