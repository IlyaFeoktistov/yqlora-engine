#include "yqlora/scene/SceneSystems.hpp"

#include "yqlora/reflection/TypeRegistry.hpp"

namespace yqlora {

SceneSystems::SceneSystems(Scene& scene, const TypeRegistry& registry)
{
    for (const auto& type : registry.types()) {
        if (type->kind() != TypeKind::System) {
            continue;
        }
        if (std::unique_ptr<SceneSystem> system = type->createSystem(scene)) {
            m_byType.emplace(type->name(), system.get());
            m_systems.push_back(std::move(system));
        }
    }
}

SceneSystems::~SceneSystems() = default;

void SceneSystems::fixedUpdate(float step)
{
    for (const auto& system : m_systems) {
        system->fixedUpdate(step);
    }
}

void SceneSystems::afterFixedUpdate(float step)
{
    for (const auto& system : m_systems) {
        system->afterFixedUpdate(step);
    }
}

void SceneSystems::branchChanged(Node& node)
{
    for (const auto& system : m_systems) {
        system->onBranchChanged(node);
    }
}

void SceneSystems::update(float deltaSeconds)
{
    for (const auto& system : m_systems) {
        system->update(deltaSeconds);
    }
}

void SceneSystems::lateUpdate(float deltaSeconds)
{
    for (const auto& system : m_systems) {
        system->lateUpdate(deltaSeconds);
    }
}

void SceneSystems::updateTransforms()
{
    for (const auto& system : m_systems) {
        system->updateTransforms();
    }
}

} // namespace yqlora
