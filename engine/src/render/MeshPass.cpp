#include "yqlora/render/MeshPass.hpp"

#include "yqlora/render/GpuMesh.hpp"
#include "yqlora/render/ShaderData.hpp"
#include "yqlora/shader/EngineShaders.hpp"
#include "yqlora/shader/ShaderBuild.hpp"

namespace yqlora {

namespace {

// Блок Material шейдера Standard (engine/shaders/standard.yshader). Пока
// материалов нет, проход заполняет его из цвета MeshRenderer.
struct alignas(16) StandardMaterialUniforms {
    Vec4 baseColor{1.0f};
    float metallic = 0.0f;
    float roughness = 0.5f;
};

const UniformLayout kStandardMaterialBlock{
    "Material",
    {kMaterialSet, 0},
    sizeof(StandardMaterialUniforms),
    {
        YQLORA_UNIFORM_MEMBER(StandardMaterialUniforms, baseColor),
        YQLORA_UNIFORM_MEMBER(StandardMaterialUniforms, metallic),
        YQLORA_UNIFORM_MEMBER(StandardMaterialUniforms, roughness),
    },
};

const UniformLayout* const kStandardLayouts[] = {&kFrameBlock, &kViewBlock, &kObjectBlock, &kStandardMaterialBlock};

const std::string kStandardShaderPath = engineShaderPath("standard.yshader");

} // namespace

MeshPass::MeshPass(const RenderContext& context)
    : m_meshes(context.meshes)
    , m_uniforms(context.uniforms)
    , m_shader(context.device.createShader(buildBuiltinShader(
          context.shaders,
          {
              .name = "Standard",
              .vertexPath = kStandardShaderPath,
              .fragmentPath = kStandardShaderPath,
              .layouts = kStandardLayouts,
          }
      )))
    , m_pipeline(context.device.createPipeline({
          .shader = m_shader.get(),
          .vertexLayout = meshVertexLayout(),
          .colorFormat = kHdrColorFormat,
      }))
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
        };
        setUniforms(commands, kObjectBlock, m_uniforms.push(object));
        // Цвет выбран в sRGB, свет считается в линейном.
        const StandardMaterialUniforms material{.baseColor = math::srgbToLinear(item->color)};
        setUniforms(commands, kStandardMaterialBlock, m_uniforms.push(material));
        drawMesh(commands, m_meshes.get(*item));
    }
}

} // namespace yqlora
