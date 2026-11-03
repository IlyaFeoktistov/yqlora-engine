#include "yqlora/render/FrameExtract.hpp"

#include "yqlora/render/LightingSettings.hpp"
#include "yqlora/scene/Scene.hpp"

namespace yqlora {

EnvironmentData extractEnvironment(const Scene& scene)
{
    return {.ambient = lightingSettingsOf(scene.services()).linearAmbient()};
}

} // namespace yqlora
