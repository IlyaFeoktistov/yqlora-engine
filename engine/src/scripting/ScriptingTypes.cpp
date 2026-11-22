#include "yqlora/scripting/ScriptingTypes.hpp"

#include "yqlora/reflection/TypeCategories.hpp"
#include "yqlora/reflection/TypeRegistry.hpp"
#include "yqlora/scripting/Script.hpp"
#include "yqlora/scripting/ScriptsSettings.hpp"

namespace yqlora {

void registerScriptingTypes(TypeRegistry& registry)
{
    // База в реестре нужна, чтобы скрипты игры встали в цепочку типов; сама
    // она не создаётся и в "Добавить компонент" не попадает.
    registry.registerComponent<Script>()
        .category(std::string(categories::kScripts))
        .guide(std::string(guides::kFirstScript));
    registry.registerSettings<ScriptsSettings>("scripts").property("configuration", &ScriptsSettings::configuration);
}

} // namespace yqlora
