#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/reflection/TypeName.hpp"
#include "yqlora/scene/SceneSystem.hpp"

#include <concepts>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace yqlora {

class Node;
class Scene;
class TypeRegistry;

// Системы одной сцены: по одной на каждый тип систем в реестре, вызываются
// в порядке регистрации.
class YQLORA_API SceneSystems {
public:
    SceneSystems(Scene& scene, const TypeRegistry& registry);
    ~SceneSystems();

    SceneSystems(const SceneSystems&) = delete;
    SceneSystems& operator=(const SceneSystems&) = delete;

    // nullptr, если такой системы в сцене нет. По имени типа, а не по typeid:
    // см. TypeRegistry::find<T>.
    template <std::derived_from<SceneSystem> T>
    T* get() const
    {
        const auto it = m_byType.find(typeNameOf<T>());

        return it != m_byType.end() ? static_cast<T*>(it->second) : nullptr;
    }

    void fixedUpdate(float step);
    void afterFixedUpdate(float step);
    void update(float deltaSeconds);
    void lateUpdate(float deltaSeconds);
    void updateTransforms();
    void branchChanged(Node& node);

private:
    std::vector<std::unique_ptr<SceneSystem>> m_systems;
    std::map<std::string, SceneSystem*, std::less<>> m_byType;
};

} // namespace yqlora
