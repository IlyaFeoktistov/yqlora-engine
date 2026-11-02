#pragma once

#include "yqlora/render/ViewPass.hpp"
#include "yqlora/scene/NodeId.hpp"

#include <memory>
#include <vector>

namespace yqlora::editor {

// Контур выбранных узлов. Сначала узлы рисуются белым во временную маску
// размером с цель, затем полноэкранный проход закрашивает пиксели рядом
// с маской. Толщина контура в пикселях не зависит от расстояния до объекта,
// а сам контур виден и сквозь другие объекты.
class OutlinePass final : public ViewPass {
public:
    explicit OutlinePass(const RenderContext& context);

    // Что обводить и каким цветом: задаётся перед каждым кадром.
    void setSelection(std::vector<NodeId> selected, const Vec4& color)
    {
        m_selected = std::move(selected);
        m_color = color;
    }

    // Два прохода: маска, затем контур поверх цвета вида. Ничего не
    // добавляет, если выбранных среди видимого нет.
    void addTo(RenderGraph& graph, const ViewPassInput& input) override;

private:
    GpuMeshCache& m_meshes;
    UniformRing& m_uniforms;
    std::unique_ptr<rhi::Sampler> m_maskSampler;
    std::unique_ptr<rhi::Shader> m_maskShader;
    std::unique_ptr<rhi::Shader> m_outlineShader;
    std::unique_ptr<rhi::Pipeline> m_maskPipeline;
    std::unique_ptr<rhi::Pipeline> m_outlinePipeline;
    std::vector<NodeId> m_selected;
    Vec4 m_color{1.0f};
};

} // namespace yqlora::editor
