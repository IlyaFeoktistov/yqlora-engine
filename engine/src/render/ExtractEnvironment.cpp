#include "yqlora/render/FrameExtract.hpp"

#include "yqlora/core/Log.hpp"
#include "yqlora/render/Environment.hpp"
#include "yqlora/render/FrameMaterials.hpp"
#include "yqlora/render/RenderSystem.hpp"

namespace yqlora {

EnvironmentData extractEnvironment(const RenderSystem& render, FrameMaterials& materials)
{
    const Environment* environment = render.environment();
    if (!environment) {
        return {};
    }

    bool misfit = false;
    EnvironmentData result{.sky = materials.sky(environment->sky.id, &misfit), .atmosphere = environment->atmosphere};
    if (misfit && render.takeReport(RenderReport::SkyMaterial)) {
        log::warn(
            "The sky material of environment '{}' has a shader that is not a sky shader: there is no sky",
            environment->name()
        );
    }

    return result;
}

} // namespace yqlora
