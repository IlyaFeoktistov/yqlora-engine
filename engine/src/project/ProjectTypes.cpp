#include "yqlora/project/ProjectTypes.hpp"

#include "yqlora/project/GeneralSettings.hpp"
#include "yqlora/reflection/TypeRegistry.hpp"
#include "yqlora/render/TextureData.hpp"
#include "yqlora/scene/SceneSerializer.hpp"

namespace yqlora {

void registerProjectTypes(TypeRegistry& registry)
{
    registry.registerSettings<SettingsSection>();

    registry.registerSettings<GeneralSettings>("general")
        .property("gameName", &GeneralSettings::gameName)
        .property("gameVersion", &GeneralSettings::gameVersion)
        .property("company", &GeneralSettings::company)
        .property(kStartupSceneProperty, &GeneralSettings::startupScene)
        .resource(SceneSerializer::kResourceKind)
        .property("icon", &GeneralSettings::icon)
        .resource(TextureData::kResourceKind);
}

} // namespace yqlora
