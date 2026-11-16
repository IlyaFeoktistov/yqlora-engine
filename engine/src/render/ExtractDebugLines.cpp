#include "yqlora/render/FrameExtract.hpp"

#include "yqlora/debug/DebugLines.hpp"
#include "yqlora/scene/Scene.hpp"

#include <span>
#include <vector>

namespace yqlora {

namespace {

void appendLinear(std::span<const DebugVertex> vertices, std::vector<DebugVertex>& target)
{
    target.reserve(target.size() + vertices.size());
    for (const DebugVertex& vertex : vertices) {
        target.push_back({vertex.position, math::srgbToLinear(vertex.color)});
    }
}

} // namespace

void extractDebugLines(Scene& scene, const DebugChannels& channels, FrameData& frame)
{
    DebugLines lines;
    scene.drawDebug(lines, channels);
    appendLinear(lines.tested(), frame.debugLines.tested);
    appendLinear(lines.overlay(), frame.debugLines.overlay);
}

} // namespace yqlora
