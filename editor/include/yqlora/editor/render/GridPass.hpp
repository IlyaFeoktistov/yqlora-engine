#pragma once

#include "yqlora/render/ViewPass.hpp"

#include <memory>

namespace yqlora::editor {

// Бесконечная сетка редактора на плоскости Y = 0. Рисуется квадрат под камерой,
// который едет вместе с ней, а линии считаются во фрагментном шейдере
// по мировым координатам. С расстоянием сетка гаснет, поэтому края квадрата не видно.
class GridPass final : public ViewPass {
public:
    explicit GridPass(const RenderContext& context);

    // Поверх сцены вида: глубину читает, но не пишет.
    void addTo(RenderGraph& graph, const ViewPassInput& input) override;

private:
    UniformRing& m_uniforms;
    std::unique_ptr<rhi::Buffer> m_corners;
    std::unique_ptr<rhi::Shader> m_shader;
    std::unique_ptr<rhi::Pipeline> m_pipeline;
};

} // namespace yqlora::editor
