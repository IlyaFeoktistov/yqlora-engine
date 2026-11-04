#include "yqlora/render/FrameBuilder.hpp"

#include "yqlora/core/EngineServices.hpp"
#include "yqlora/render/FrameExtract.hpp"
#include "yqlora/render/FrameMaterials.hpp"
#include "yqlora/render/RenderSystem.hpp"
#include "yqlora/scene/Scene.hpp"

namespace yqlora {

FrameData extractFrame(const Scene& scene, float aspect)
{
    FrameData frame;
    FrameMaterials materials(scene.services().resources(), frame);
    if (const RenderSystem* render = scene.system<RenderSystem>()) {
        frame.views = extractViews(*render, aspect);
        // Ближайшие источники - к первой камере: она рисуется первой и обычно главная.
        const Vec3 camera = frame.views.empty() ? Vec3(0.0f) : frame.views.front().camera.position;
        frame.lights = extractLights(*render, camera);
        frame.environment = extractEnvironment(*render, materials);
    }
    extractDrawItems(scene, materials, frame);

    return frame;
}

} // namespace yqlora
