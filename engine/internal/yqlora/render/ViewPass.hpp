#pragma once

#include "yqlora/render/FrameData.hpp"
#include "yqlora/render/RenderGraph.hpp"
#include "yqlora/render/UniformRing.hpp"

#include <span>

namespace yqlora {

class GpuMeshCache;
class ShaderCompiler;

// Что рендерер даёт проходам, в том числе чужим: устройство, компилятор
// шейдеров движка, uniform-данные кадра и меши в видеопамяти.
struct RenderContext {
    rhi::Device& device;
    ShaderCompiler& shaders;
    UniformRing& uniforms;
    GpuMeshCache& meshes;
};

// Вид кадра, каким его видит дополнительный проход.
struct ViewPassInput {
    const FrameData& frame;
    const ViewData& view;
    // Что вид рисует, в порядке Prepare.
    std::span<const DrawItem* const> visible;
    UniformSlice viewUniforms;
    // Цвет и глубина, куда нарисован вид.
    RenderGraph::TextureId color = 0;
    RenderGraph::TextureId depth = 0;
};

// Проход, который владелец вида добавляет к нему (ViewData::passes):
// например, сетка и контур выделения редактора. Проход держит свои данные
// сам, движок только добавляет его в граф после сцены вида. Живёт, пока
// жив рендерер, на котором создан.
class ViewPass {
public:
    virtual ~ViewPass() = default;

    virtual void addTo(RenderGraph& graph, const ViewPassInput& input) = 0;
};

} // namespace yqlora
