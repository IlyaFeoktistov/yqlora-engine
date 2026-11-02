#include "yqlora/render/MeshPass.hpp"

#include "yqlora/render/GpuMesh.hpp"
#include "yqlora/render/ShaderData.hpp"
#include "yqlora/shader/EngineShaders.hpp"
#include "yqlora/shader/ShaderBuild.hpp"

namespace yqlora {

MeshPass::MeshPass(rhi::Device& device, ShaderCompiler& shaders, GpuMeshCache& meshes, UniformRing& uniforms)
    : m_meshes(meshes)
    , m_uniforms(uniforms)
    , m_shader(device.createShader(buildEngineShader(
          shaders,
          {
              .name = "mesh",
              .vertexPath = engineShaderPath("mesh.vert"),
              .fragmentPath = engineShaderPath("mesh.frag"),
              .layouts = kEngineBlocks,
          }
      )))
    , m_pipeline(device.createPipeline({.shader = m_shader.get(), .vertexLayout = meshVertexLayout()}))
{
}

void MeshPass::draw(
    rhi::CommandList& commands,
    const FrameData& frame,
    const ViewData& view,
    const UniformSlice& frameUniforms,
    const UniformSlice& viewUniforms
) const
{
    if (frame.drawItems.empty()) {
        return;
    }
    commands.bindPipeline(*m_pipeline);
    setUniforms(commands, kFrameBlock, frameUniforms);
    setUniforms(commands, kViewBlock, viewUniforms);
    for (const DrawItem& item : frame.drawItems) {
        if (!inMask(view.mask, item.layer)) {
            continue;
        }
        // Нормали поворачиваются вместе с объектом, но неравномерный масштаб
        // их бы исказил: для этого и нужна обратная транспонированная матрица.
        const ObjectUniforms object{
            .model = item.world,
            .normalMatrix = Mat4(glm::transpose(glm::inverse(Mat3(item.world)))),
            .color = item.color,
        };
        setUniforms(commands, kObjectBlock, m_uniforms.push(object));
        drawMesh(commands, m_meshes.get(item));
    }
}

} // namespace yqlora
