#include "yqlora/render/EnvironmentLighting.hpp"

#include "yqlora/render/EngineUniforms.hpp"
#include "yqlora/render/MaterialBinding.hpp"
#include "yqlora/render/MaterialBlock.hpp"
#include "yqlora/render/PipelineCache.hpp"
#include "yqlora/render/ViewPass.hpp"
#include "yqlora/shader/EngineShaders.hpp"
#include "yqlora/shader/ShaderBuild.hpp"
#include "yqlora/shader/ShaderProfile.hpp"

#include <algorithm>
#include <cmath>
#include <span>
#include <string>

namespace yqlora {

namespace {

// Свет неба - линейная яркость в нитах, больше 1.
constexpr rhi::Format kFormat = rhi::Format::Rgba16Float;
constexpr std::uint32_t kFullscreenTriangleVertices = 3;
// Куб, из которого читают проекция и фильтрация (их шейдеры, набор 2).
constexpr rhi::BindingSlot kSourceSlot{kMaterialSet, 0};
constexpr rhi::BindingSlot kPrefilterBlockSlot{kMaterialSet, 1};

// Повторяет блок Prefilter в engine/shaders/environment/prefilter.frag.
struct alignas(16) PrefilterUniforms {
    int face = 0;
    float size = 0.0f;
    float roughness = 0.0f;
};

const UniformLayout kPrefilterBlock{
    "Prefilter",
    kPrefilterBlockSlot,
    sizeof(PrefilterUniforms),
    {
        YQLORA_UNIFORM_MEMBER(PrefilterUniforms, face),
        YQLORA_UNIFORM_MEMBER(PrefilterUniforms, size),
        YQLORA_UNIFORM_MEMBER(PrefilterUniforms, roughness),
    },
};

const UniformLayout* const kPrefilterLayouts[] = {&kPrefilterBlock};

rhi::TextureDesc cubeDesc()
{
    return {
        .width = EnvironmentLighting::kCaptureSize,
        .height = EnvironmentLighting::kCaptureSize,
        .format = kFormat,
        .usage = rhi::kColorTarget | rhi::kSampled,
        .mipmaps = true,
        .cube = true,
    };
}

} // namespace

EnvironmentLighting::EnvironmentLighting(const RenderContext& context)
    : m_context(context)
    , m_sampler(
          context.device.createSampler({.filter = rhi::Filter::Linear, .wrap = rhi::Wrap::Clamp, .mipmaps = true})
      )
    , m_harmonicsPass(makePass("environment/harmonics.frag"))
    , m_prefilterPass(makePass("environment/prefilter.frag"))
    , m_brdfPass(makePass("environment/brdf.frag"))
    , m_capture(context.device.createTexture(cubeDesc()))
    , m_brdf(context.device.createTexture({
          .width = kBrdfSize,
          .height = kBrdfSize,
          .format = kFormat,
          .usage = rhi::kColorTarget | rhi::kSampled,
      }))
{
}

EnvironmentLighting::~EnvironmentLighting() = default;

EnvironmentLighting::Pass EnvironmentLighting::makePass(const char* file)
{
    Pass pass;
    pass.shader = m_context.device.createShader(buildBuiltinShader(
        m_context.shaders,
        {
            .name = file,
            .vertexPath = engineShaderPath("fullscreen.vert"),
            .fragmentPath = engineShaderPath(file),
            .layouts = kPrefilterLayouts,
        }
    ));
    pass.pipeline = m_context.device.createPipeline({
        .shader = pass.shader.get(),
        .depthTest = false,
        .depthWrite = false,
        .cull = rhi::CullMode::None,
        .colorFormat = kFormat,
        .depthFormat = rhi::Format::None,
    });

    return pass;
}

std::optional<EnvironmentLighting::Key> EnvironmentLighting::keyOf(const FrameData& frame)
{
    const EnvironmentData& environment = frame.environment;
    if (!environment.ambientSky) {
        return std::nullopt;
    }
    const FrameMaterial& material = frame.materials[*environment.ambientSky];
    const bool overridden = environment.ambientOverrides != DrawItem::kNoOverrides;
    const FrameOverrides* overrides = overridden ? &frame.overrides[environment.ambientOverrides] : nullptr;
    Key key{
        .material = material.id,
        .shader = material.shaderId,
        .shaderRevision = material.shaderRevision,
        .block = materialBlock(*material.shader, *material.data, overrides ? &overrides->values : nullptr),
        .atmosphere = atmosphereUniforms(environment.atmosphere),
    };
    for (const FrameTexture& texture : overrides ? overrides->textures : material.textures) {
        key.textures.emplace_back(texture.id, texture.revision);
    }
    // Солнце, атмосфера и высота камеры: из них небо атмосферы.
    const FrameUniforms uniforms = frameUniforms(frame);
    key.hasSun = uniforms.hasSun;
    key.sunDirection = uniforms.sunDirection;
    key.sunColor = uniforms.sunColor;
    const float height = frame.views.empty() ? 0.0f : frame.views.front().camera.position.y;
    key.heightStep = static_cast<int>(std::floor(height / kHeightStep));

    return key;
}

void EnvironmentLighting::addCapture(
    RenderGraph& graph,
    const FrameData& frame,
    const EngineResources& resources,
    const EngineTextureIds& atmosphere,
    const Textures& textures,
    RenderGraph::TextureId capture
)
{
    const EnvironmentData& environment = frame.environment;
    const FrameMaterial& material = frame.materials[*environment.ambientSky];
    const bool overridden = environment.ambientOverrides != DrawItem::kNoOverrides;
    const FrameOverrides* overrides = overridden ? &frame.overrides[environment.ambientOverrides] : nullptr;
    for (int face = 0; face < rhi::kCubeFaces; ++face) {
        const UniformSlice slice = m_context.uniforms.push(
            CaptureUniforms{.face = face, .size = static_cast<float>(kCaptureSize)}
        );
        graph.addPass({
            .name = "environment capture",
            .color = {capture, rhi::LoadOp::Load, face},
            .reads = atmosphere.reads(),
            .record = [this, &material, overrides, resources, atmosphere, slice](
                          rhi::CommandList& commands,
                          const RenderGraph& passes
                      ) {
                const rhi::Pipeline* pipeline = m_context.pipelines
                                                    .get(material, MaterialPass::Capture, kFormat, rhi::Format::None);
                if (!pipeline) {
                    return;
                }
                commands.bindPipeline(*pipeline);
                bindEngineResources(commands, *pipeline->desc().shader, withTextures(resources, passes, atmosphere));
                setUniforms(commands, kCaptureBlock, slice);
                bindMaterial(commands, m_context, *pipeline->desc().shader, material, overrides);
                commands.draw(kFullscreenTriangleVertices);
            },
        });
    }
    graph.addPass({.name = "environment capture mipmaps", .color = {capture}, .generateMipmaps = true});

    graph.addPass({
        .name = "environment harmonics",
        .color = {textures.harmonics, rhi::LoadOp::Load},
        .reads = {capture},
        .record = [this, capture](rhi::CommandList& commands, const RenderGraph& passes) {
            commands.bindPipeline(*m_harmonicsPass.pipeline);
            commands.setTexture(kSourceSlot.set, kSourceSlot.binding, passes.texture(capture), *m_sampler);
            commands.draw(kFullscreenTriangleVertices);
        },
    });

    for (int mip = 0; mip < kSpecularMips; ++mip) {
        for (int face = 0; face < rhi::kCubeFaces; ++face) {
            const UniformSlice slice = m_context.uniforms.push(
                PrefilterUniforms{
                    .face = face,
                    .size = static_cast<float>(rhi::mipSize(kCaptureSize, mip)),
                    .roughness = static_cast<float>(mip) / kMaxSpecularMip,
                }
            );
            graph.addPass({
                .name = "environment prefilter",
                .color = {textures.specular, rhi::LoadOp::Load, face, mip},
                .reads = {capture},
                .record = [this, capture, slice](rhi::CommandList& commands, const RenderGraph& passes) {
                    commands.bindPipeline(*m_prefilterPass.pipeline);
                    commands.setTexture(kSourceSlot.set, kSourceSlot.binding, passes.texture(capture), *m_sampler);
                    setUniforms(commands, kPrefilterBlock, slice);
                    commands.draw(kFullscreenTriangleVertices);
                },
            });
        }
    }
}

void EnvironmentLighting::addClear(RenderGraph& graph, const Textures& textures)
{
    const Vec4 black(0.0f, 0.0f, 0.0f, 1.0f);
    graph.addPass(
        {.name = "environment clear", .color = {textures.harmonics, rhi::LoadOp::Clear}, .clearColor = black}
    );
    for (int mip = 0; mip < kSpecularMips; ++mip) {
        for (int face = 0; face < rhi::kCubeFaces; ++face) {
            graph.addPass({
                .name = "environment clear",
                .color = {textures.specular, rhi::LoadOp::Clear, face, mip},
                .clearColor = black,
            });
        }
    }
}

EnvironmentLighting::Textures EnvironmentLighting::addTo(
    RenderGraph& graph,
    const FrameData& frame,
    const EngineResources& resources,
    const EngineTextureIds& atmosphere
)
{
    Textures textures{.brdf = graph.import(*m_brdf)};
    if (!m_brdfReady) {
        m_brdfReady = true;
        graph.addPass({
            .name = "environment brdf",
            .color = {textures.brdf, rhi::LoadOp::Load},
            .record = [this](rhi::CommandList& commands, const RenderGraph&) {
                commands.bindPipeline(*m_brdfPass.pipeline);
                commands.draw(kFullscreenTriangleVertices);
            },
        });
    }

    std::optional<Key> key = keyOf(frame);
    bool built = false;
    Set& set = setFor(key, built);
    textures.harmonics = graph.import(*set.harmonics);
    textures.specular = graph.import(*set.specular);
    if (built) {
        return textures;
    }
    if (!key) {
        addClear(graph, textures);
    } else {
        ++m_captures;
        addCapture(graph, frame, resources, atmosphere, textures, graph.import(*m_capture));
    }
    set.key = std::move(key);

    return textures;
}

EnvironmentLighting::Set& EnvironmentLighting::setFor(const std::optional<Key>& key, bool& built)
{
    const std::uint64_t now = ++m_uses;
    const auto found = std::ranges::find(m_sets, key, &Set::key);
    if (found != m_sets.end()) {
        found->lastUsed = now;
        built = true;

        return *found;
    }
    built = false;
    if (m_sets.size() < kSets) {
        m_sets.push_back({
            .specular = m_context.device.createTexture(cubeDesc()),
            .harmonics = m_context.device.createTexture({
                .width = kAmbientHarmonicsCount,
                .height = 1,
                .format = kFormat,
                .usage = rhi::kColorTarget | rhi::kSampled,
            }),
            .lastUsed = now,
        });

        return m_sets.back();
    }
    Set& oldest = *std::ranges::min_element(m_sets, {}, &Set::lastUsed);
    oldest.lastUsed = now;

    return oldest;
}

} // namespace yqlora
