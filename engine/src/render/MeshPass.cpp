#include "yqlora/render/MeshPass.hpp"

#include "yqlora/core/Log.hpp"
#include "yqlora/render/EngineUniforms.hpp"
#include "yqlora/render/FrameFormats.hpp"
#include "yqlora/render/GpuMaterial.hpp"
#include "yqlora/render/GpuMesh.hpp"
#include "yqlora/render/GpuTexture.hpp"
#include "yqlora/render/MaterialBlock.hpp"
#include "yqlora/render/PipelineCache.hpp"
#include "yqlora/resource/BuiltinResourceTable.hpp"
#include "yqlora/shader/ShaderProfile.hpp"

namespace yqlora {

MeshPass::MeshPass(const RenderContext& context)
    : m_context(context)
    , m_errorShaderData(findBuiltinResource(builtin::kErrorShader)->make())
    , m_errorMaterialData(findBuiltinResource(builtin::kErrorMaterial)->make())
    , m_error{
          .id = builtin::kErrorMaterial,
          .data = static_cast<const MaterialData*>(m_errorMaterialData.get()),
          .shaderId = builtin::kErrorShader,
          .shader = static_cast<const ShaderData*>(m_errorShaderData.get()),
      }
{
}

MeshPass::~MeshPass() = default;

void MeshPass::draw(
    rhi::CommandList& commands,
    const FrameData& frame,
    std::span<const DrawItem* const> visible,
    const UniformSlice& frameUniforms,
    const UniformSlice& viewUniforms
)
{
    const rhi::Pipeline* bound = nullptr;
    for (const DrawItem* item : visible) {
        const FrameMaterial* material = &frame.materials[item->material];
        const rhi::Pipeline* pipeline = m_context.pipelines.get(*material, kHdrColorFormat, kDepthFormat);
        if (!pipeline) {
            material = &m_error;
            pipeline = m_context.pipelines.get(*material, kHdrColorFormat, kDepthFormat);
        }
        if (!pipeline) {
            continue;
        }
        if (pipeline != bound) {
            commands.bindPipeline(*pipeline);
            setUniforms(commands, kFrameBlock, frameUniforms);
            setUniforms(commands, kViewBlock, viewUniforms);
            bound = pipeline;
        }

        const bool overridden = item->overrides != DrawItem::kNoOverrides && material != &m_error;
        const FrameOverrides* overrides = overridden ? &frame.overrides[item->overrides] : nullptr;
        if (overrides && material->shader->materialSize != 0) {
            // Замены - свои данные материала у этого элемента на этот кадр.
            const auto block = materialBlock(*material->shader, *material->data, &overrides->values, [&](auto name) {
                if (m_reportedOverrides.emplace(material->shaderId, std::string(name)).second) {
                    log::warn(
                        "Material override '{}' does not fit shader '{}' and is skipped",
                        name,
                        material->shader->desc.name
                    );
                }
            });
            const UniformSlice slice = m_context.uniforms.push(std::span<const std::byte>(block));
            commands
                .setUniformBuffer(kMaterialSet, kMaterialBlockSlot.binding, *slice.buffer, slice.offset, slice.size);
        } else if (const rhi::Buffer* block = m_context.materials.block(*material)) {
            commands.setUniformBuffer(kMaterialSet, kMaterialBlockSlot.binding, *block, 0, block->desc().size);
        }
        for (const FrameTexture& texture : overrides ? overrides->textures : material->textures) {
            commands.setTexture(
                texture.slot.set,
                texture.slot.binding,
                m_context.textures.texture(texture),
                m_context.textures.sampler(texture)
            );
        }

        // Нормали поворачиваются вместе с объектом, но неравномерный масштаб
        // их бы исказил: для этого и нужна обратная транспонированная матрица.
        const ObjectUniforms object{
            .model = item->world,
            .normalMatrix = Mat4(glm::transpose(glm::inverse(Mat3(item->world)))),
        };
        setUniforms(commands, kObjectBlock, m_context.uniforms.push(object));
        drawMesh(commands, m_context.meshes.get(*item), item->firstIndex, item->indexCount);
    }
}

} // namespace yqlora
