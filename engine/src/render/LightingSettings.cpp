#include "yqlora/render/LightingSettings.hpp"

#include "yqlora/core/EngineServices.hpp"
#include "yqlora/project/Project.hpp"

#include <algorithm>

namespace yqlora {

void LightingSettings::setAmbientIntensity(float intensity)
{
    m_ambientIntensity = std::max(intensity, 0.0f);
}

Vec3 LightingSettings::linearAmbient() const
{
    return math::srgbToLinear(ambientColor) * m_ambientIntensity;
}

const LightingSettings& lightingSettingsOf(const EngineServices& services)
{
    static const LightingSettings defaults;
    const Project* project = services.project();
    const SettingsSection* section = project ? project->settings().find(typeNameOf<LightingSettings>()) : nullptr;

    return section ? static_cast<const LightingSettings&>(*section) : defaults;
}

} // namespace yqlora
