#include "yqlora/editor/ComponentSetup.hpp"

#include "yqlora/editor/ColliderEditing.hpp"
#include "yqlora/physics/Collider.hpp"
#include "yqlora/reflection/TypeRegistry.hpp"
#include "yqlora/scene/Component.hpp"
#include "yqlora/scene/Node3D.hpp"

#include <algorithm>

namespace yqlora::editor {

namespace {

bool derivesFrom(const TypeRegistry& registry, const TypeInfo& type, const TypeInfo* base)
{
    if (!base) {
        return false;
    }
    const auto chain = registry.chain(type);

    return std::ranges::find(chain, base) != chain.end();
}

} // namespace

std::vector<PropertyEdit> addedComponentSetup(
    const TypeRegistry& registry,
    const TypeInfo& type,
    const Node& node,
    ResourceCache& resources
)
{
    const auto* spatial = dynamic_cast<const Node3D*>(&node);
    // Компонент создаётся только у типов с настройкой: конструктор скрипта
    // игры зря не вызывается.
    if (!spatial || !derivesFrom(registry, type, registry.find<Collider>())) {
        return {};
    }
    const std::unique_ptr<Component> component = type.createComponent();
    auto* collider = dynamic_cast<Collider*>(component.get());
    if (!collider) {
        return {};
    }

    const ComponentSnapshot fresh(registry, *collider);
    fitCollider(*collider, *spatial, resources);

    return fresh.changes(*collider);
}

} // namespace yqlora::editor
