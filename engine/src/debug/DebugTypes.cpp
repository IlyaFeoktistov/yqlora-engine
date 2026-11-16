#include "yqlora/debug/DebugTypes.hpp"

#include "yqlora/debug/DebugDraw.hpp"
#include "yqlora/reflection/TypeRegistry.hpp"

namespace yqlora {

void registerDebugTypes(TypeRegistry& registry)
{
    registry.registerSystem<DebugDraw>();
}

} // namespace yqlora
