#include "yqlora/rhi/Pipeline.hpp"

#include <stdexcept>

namespace yqlora::rhi {

void checkPipelineTarget(const PipelineDesc& pipeline, const TargetFormats& target)
{
    const std::string name = pipeline.shader ? pipeline.shader->desc().name : std::string("?");
    if (pipeline.colorFormat != target.color) {
        throw std::logic_error("Pipeline '" + name + "': colour format differs from the render pass");
    }
    if (pipeline.depthFormat != target.depth) {
        throw std::logic_error("Pipeline '" + name + "': depth format differs from the render pass");
    }
    if (pipeline.samples != target.samples) {
        throw std::logic_error("Pipeline '" + name + "': sample count differs from the render pass");
    }
}

} // namespace yqlora::rhi
