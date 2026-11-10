#include "yqlora/render/FrameBuilder.hpp"

#include "yqlora/core/EngineServices.hpp"
#include "yqlora/render/FrameExtract.hpp"
#include "yqlora/render/FrameMaterials.hpp"
#include "yqlora/render/GraphicsSettings.hpp"
#include "yqlora/render/RenderSystem.hpp"
#include "yqlora/scene/Scene.hpp"

#include <algorithm>
#include <vector>

namespace yqlora {

namespace {

bool directional(const LightData& light)
{
    return light.kind == LightKind::Directional;
}

} // namespace

FrameData extractFrame(const Scene& scene, float aspect, const Scene* lighting)
{
    FrameData frame;
    FrameMaterials materials(scene.services().resources(), frame);
    frame.shadows = shadowSettings(graphicsSettingsOf(scene.services()));
    const RenderSystem* fallback = lighting ? lighting->system<RenderSystem>() : nullptr;
    if (const RenderSystem* render = scene.system<RenderSystem>()) {
        frame.views = extractViews(*render, aspect);
        // Ближайшие источники и тени - к первой камере: она рисуется первой и обычно главная.
        const Vec3 camera = frame.views.empty() ? Vec3(0.0f) : frame.views.front().camera.position;
        frame.lights = extractLights(*render, camera);
        if (fallback && std::ranges::none_of(frame.lights, directional)) {
            // Направленные идут первыми (солнце - самым первым), как у extractLights.
            std::vector<LightData> lights = extractLights(*fallback, camera);
            std::erase_if(lights, [](const LightData& light) {
                return !directional(light);
            });
            frame.lights.insert(frame.lights.begin(), lights.begin(), lights.end());
        }
        extractShadowTiles(*render, camera, frame);
        const bool own = render->environment() != nullptr;
        frame.environment = extractEnvironment(own || !fallback ? *render : *fallback, materials);
    }
    extractDrawItems(scene, materials, frame);

    return frame;
}

} // namespace yqlora
