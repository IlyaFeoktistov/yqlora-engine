#include "yqlora/core/EngineTypes.hpp"

#include "yqlora/input/InputTypes.hpp"
#include "yqlora/physics/PhysicsTypes.hpp"
#include "yqlora/project/ProjectTypes.hpp"
#include "yqlora/render/RenderTypes.hpp"
#include "yqlora/scene/SceneTypes.hpp"
#include "yqlora/scripting/ScriptingTypes.hpp"

namespace yqlora {

void registerEngineTypes(TypeRegistry& registry)
{
    // Первыми: корень разделов настроек нужен разделам подсистем.
    registerProjectTypes(registry);
    registerInputTypes(registry);
    registerSceneTypes(registry);
    registerRenderTypes(registry);
    registerPhysicsTypes(registry);
    registerScriptingTypes(registry);
}

} // namespace yqlora
