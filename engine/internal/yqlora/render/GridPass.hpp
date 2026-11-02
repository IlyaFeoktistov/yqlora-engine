#pragma once

#include "yqlora/render/UniformRing.hpp"
#include "yqlora/rhi/Device.hpp"

#include <memory>

namespace yqlora {

class ShaderCompiler;

// Бесконечная сетка редактора на плоскости Y = 0. Рисуется квадрат под камерой,
// который едет вместе с ней, а линии считаются во фрагментном шейдере
// по мировым координатам. С расстоянием сетка гаснет, поэтому края квадрата не видно.
class GridPass {
public:
    GridPass(rhi::Device& device, ShaderCompiler& shaders, UniformRing& uniforms);

    // Рисует в открытый проход поверх сцены: глубину читает, но не пишет.
    void draw(rhi::CommandList& commands, const UniformSlice& viewUniforms) const;

private:
    UniformRing& m_uniforms;
    std::unique_ptr<rhi::Buffer> m_corners;
    std::unique_ptr<rhi::Shader> m_shader;
    std::unique_ptr<rhi::Pipeline> m_pipeline;
};

} // namespace yqlora
