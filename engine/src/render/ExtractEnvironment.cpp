#include "yqlora/render/FrameExtract.hpp"

#include "yqlora/core/Log.hpp"
#include "yqlora/render/Environment.hpp"
#include "yqlora/render/FrameMaterials.hpp"
#include "yqlora/render/RenderSystem.hpp"
#include "yqlora/resource/BuiltinResources.hpp"

namespace yqlora {

namespace {

// Поля встроенного неба Gradient (gradient_sky.yshader), которые задаёт окружение.
constexpr char kGradientSky[] = "skyColor";
constexpr char kGradientEquator[] = "equatorColor";
constexpr char kGradientGround[] = "groundColor";
constexpr char kGradientLuminance[] = "luminance";

// Цвета градиента для источника Gradient или один цвет на всё для Color.
MaterialOverrides gradientOverrides(const Environment& environment)
{
    const bool single = environment.ambientSource == AmbientSource::Color;
    MaterialOverrides overrides;
    overrides.setColor(kGradientSky, single ? environment.ambientColor : environment.skyColor);
    overrides.setColor(kGradientEquator, single ? environment.ambientColor : environment.equatorColor);
    overrides.setColor(kGradientGround, single ? environment.ambientColor : environment.groundColor);
    overrides.setFloat(kGradientLuminance, environment.gradientLuminance());

    return overrides;
}

} // namespace

EnvironmentData extractEnvironment(const RenderSystem& render, FrameMaterials& materials)
{
    const Environment* environment = render.environment();
    if (!environment) {
        return {};
    }

    bool misfit = false;
    EnvironmentData result{
        .sky = materials.sky(environment->sky.id, &misfit),
        .ambientIntensity = environment->ambientIntensity(),
        .atmosphere = environment->atmosphere,
    };
    if (environment->ambientSource == AmbientSource::Sky) {
        result.ambientSky = result.sky;
    } else {
        result.ambientSky = materials.sky(builtin::kGradientSkyMaterial);
        result.ambientOverrides = materials.addOverrides(*result.ambientSky, gradientOverrides(*environment));
    }
    if (misfit && render.takeReport(RenderReport::SkyMaterial)) {
        log::warn(
            "The sky material of environment '{}' has a shader that is not a sky shader: there is no sky",
            environment->name()
        );
    }

    return result;
}

} // namespace yqlora
