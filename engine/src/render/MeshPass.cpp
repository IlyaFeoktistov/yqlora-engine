#include "yqlora/render/MeshPass.hpp"

#include "yqlora/render/GpuMesh.hpp"
#include "yqlora/render/ShaderData.hpp"
#include "yqlora/shader/EngineShaders.hpp"
#include "yqlora/shader/ShaderBuild.hpp"

namespace yqlora {

MeshPass::MeshPass(const RenderContext& context)
    : m_meshes(context.meshes)
    , m_uniforms(context.uniforms)
    , m_shader(context.device.createShader(buildBuiltinShader(
          context.shaders,
          {
              .name = "mesh",
              .vertexPath = engineShaderPath("mesh.vert"),
              .fragmentPath = engineShaderPath("mesh.frag"),
              .layouts = kEngineBlocks,
          }
      )))
    , m_pipeline(context.device.createPipeline({.shader = m_shader.get(), .vertexLayout = meshVertexLayout()}))
{
}

void MeshPass::draw(
    rhi::CommandList& commands,
    std::span<const DrawItem* const> visible,
    const UniformSlice& frameUniforms,
    const UniformSlice& viewUniforms
) const
{
    if (visible.empty()) {
        return;
    }
    commands.bindPipeline(*m_pipeline);
    setUniforms(commands, kFrameBlock, frameUniforms);
    setUniforms(commands, kViewBlock, viewUniforms);
    for (const DrawItem* item : visible) {
        // Нормали поворачиваются вместе с объектом, но неравномерный масштаб
        // их бы исказил: для этого и нужна обратная транспонированная матрица.
        const ObjectUniforms object{
            .model = item->world,
            .normalMatrix = Mat4(glm::transpose(glm::inverse(Mat3(item->world)))),
            .color = item->color,
        };
        setUniforms(commands, kObjectBlock, m_uniforms.push(object));
        drawMesh(commands, m_meshes.get(*item));
    }
}

} // namespace yqlora
