#include "yqlora/render/FrameBuilder.hpp"
#include "yqlora/render/FrameExtract.hpp"

#include "yqlora/render/RenderSystem.hpp"

namespace yqlora {

std::vector<ViewData> extractViews(const RenderSystem& render, float aspect)
{
    std::vector<ViewData> views;
    for (const Camera3D* camera : render.cameras()) {
        views.push_back(cameraView(*camera, aspect));
    }

    return views;
}

ViewData cameraView(const Camera3D& camera, float aspect)
{
    return {
        .camera =
            {
                .view = camera.viewMatrix(),
                .projection = camera.projectionMatrix(aspect),
                .position = camera.worldPosition(),
            },
        .clear = camera.clear(),
        .background = math::srgbToLinear(camera.background()),
        .mask = camera.cullingMask(),
    };
}

} // namespace yqlora
