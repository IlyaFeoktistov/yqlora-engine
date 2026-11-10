#include "yqlora/scene/PrefabPool.hpp"

#include "yqlora/core/JsonId.hpp"
#include "yqlora/core/Log.hpp"
#include "yqlora/reflection/PropertyJson.hpp"
#include "yqlora/reflection/TypeRegistry.hpp"
#include "yqlora/scene/MissingComponent.hpp"
#include "yqlora/scene/Node.hpp"
#include "yqlora/scene/PrefabExpansion.hpp"
#include "yqlora/scene/PrefabInstance.hpp"
#include "yqlora/scene/Scene.hpp"
#include "yqlora/scene/SceneBranches.hpp"
#include "yqlora/scene/SceneFileKeys.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cassert>

namespace yqlora {

namespace {

using Json = nlohmann::json;
namespace keys = scene_keys;

const Json& arrayAt(const Json& object, const char* key)
{
    static const Json kEmpty = Json::array();
    const auto it = object.find(key);

    return it != object.end() && it->is_array() ? *it : kEmpty;
}

const Json& propertiesOf(const Json& object)
{
    static const Json kEmpty = Json::object();
    const auto it = object.find(keys::kProperties);

    return it != object.end() ? *it : kEmpty;
}

NodeId idOf(const Json& object)
{
    const auto it = object.find(keys::kId);

    return it != object.end() ? idFromJson(*it).value_or(0) : 0;
}

// Дерево экземпляра то же, что у только что развёрнутого: те же узлы и
// компоненты в том же порядке.
bool sameTree(const Node& node, const Json& fresh)
{
    const Json& components = arrayAt(fresh, keys::kComponents);
    const Json& children = arrayAt(fresh, keys::kChildren);
    if (node.id() != idOf(fresh) || node.components().size() != components.size()
        || node.children().size() != children.size()) {
        return false;
    }
    for (std::size_t i = 0; i < components.size(); ++i) {
        if (node.components()[i]->id() != idOf(components[i])) {
            return false;
        }
    }
    for (std::size_t i = 0; i < children.size(); ++i) {
        if (!sameTree(*node.children()[i], children[i])) {
            return false;
        }
    }

    return true;
}

// Значения свойств - как у только что развёрнутого экземпляра.
void resetValues(const TypeRegistry& registry, Node& node, const Json& fresh)
{
    if (const TypeInfo* type = registry.find(node.typeName())) {
        propertiesFromJson(registry, *type, node, propertiesOf(fresh));
    }
    const Json& components = arrayAt(fresh, keys::kComponents);
    for (std::size_t i = 0; i < components.size(); ++i) {
        Component& component = *node.components()[i];
        const TypeInfo* type = registry.find(component.typeName());
        if (type && !dynamic_cast<const MissingComponent*>(&component)) {
            propertiesFromJson(registry, *type, component, propertiesOf(components[i]));
        }
    }
    const Json& children = arrayAt(fresh, keys::kChildren);
    for (std::size_t i = 0; i < children.size(); ++i) {
        resetValues(registry, *node.children()[i], children[i]);
    }
}

} // namespace

struct PrefabPool::Slot {
    NodeId root = 0;
    // Ветка экземпляра, как он развёрнут: к ней он возвращается.
    Json fresh;
    bool free = true;
};

PrefabPool::PrefabPool() = default;

PrefabPool::~PrefabPool() = default;

void PrefabPool::onStart()
{
    fill();
}

void PrefabPool::fill()
{
    if (m_filled) {
        return;
    }
    m_filled = true;
    for (int i = 0; i < size; ++i) {
        if (!create()) {
            break;
        }
    }
}

bool PrefabPool::create()
{
    Scene& scene = *owner()->scene();
    const TypeRegistry& registry = scene.services().registry();
    auto node = SceneBranches::instantiate(registry, prefab.id, PrefabSources::of(scene.services()));
    if (!node) {
        log::error("PrefabPool on '{}': the prefab is not expanded, no instance is made", owner()->name());

        return false;
    }
    // Выключен до входа в сцену: его компоненты не начинают игру.
    node->setActive(false);
    const PrefabInstance& instance = *node->prefabInstance();
    Slot slot{node->id(), expandPrefab(*instance.base, Json::object(), instance.id, registry).tree, true};
    owner()->addChild(std::move(node));
    m_slots.push_back(std::move(slot));

    return true;
}

void PrefabPool::forgetDestroyed()
{
    const Scene& scene = *owner()->scene();
    std::erase_if(m_slots, [&](const Slot& slot) {
        return !scene.find(slot.root);
    });
}

Node* PrefabPool::acquire(Node& parent)
{
    Scene* scene = owner() ? owner()->scene() : nullptr;
    assert(scene && parent.scene() == scene && "PrefabPool::acquire: the pool and the parent are not in one scene");
    fill();
    forgetDestroyed();
    auto it = std::ranges::find_if(m_slots, &Slot::free);
    if (it == m_slots.end()) {
        if (whenEmpty == PoolWhenEmpty::Refuse || !create()) {
            return nullptr;
        }
        it = std::prev(m_slots.end());
    }
    Node& node = *scene->find(it->root);
    it->free = false;
    node.setActive(true);
    if (node.parent() != &parent) {
        scene->setParent(node, parent, KeepWorld::No);
    }

    return &node;
}

Node* PrefabPool::acquire(const NodeRef& parent)
{
    Node* found = owner() && owner()->scene() ? owner()->scene()->find(parent) : nullptr;
    if (!found) {
        log::error("PrefabPool::acquire: the parent node is not in the scene");

        return nullptr;
    }

    return acquire(*found);
}

bool PrefabPool::release(Node& node)
{
    Scene* scene = owner() ? owner()->scene() : nullptr;
    assert(scene && "PrefabPool::release: the pool is not in a scene");
    forgetDestroyed();
    const auto it = std::ranges::find_if(m_slots, [&](const Slot& slot) {
        return !slot.free && slot.root == node.id();
    });
    if (it == m_slots.end()) {
        log::error("PrefabPool on '{}': node '{}' is not handed out by this pool", owner()->name(), node.name());

        return false;
    }
    if (!sameTree(node, it->fresh)) {
        // Вернуть изменённое дерево к префабу - то же развёртывание, что новый экземпляр.
        scene->destroy(node);
        m_slots.erase(it);
        create();

        return true;
    }
    resetValues(scene->services().registry(), node, it->fresh);
    node.setActive(false);
    if (node.parent() != owner()) {
        scene->setParent(node, *owner(), KeepWorld::No);
    }
    it->free = true;

    return true;
}

bool PrefabPool::release(const NodeRef& node)
{
    Node* found = owner() && owner()->scene() ? owner()->scene()->find(node) : nullptr;
    if (!found) {
        log::error("PrefabPool::release: the node is not in the scene");

        return false;
    }

    return release(*found);
}

int PrefabPool::available() const
{
    const Scene* scene = owner() ? owner()->scene() : nullptr;

    return static_cast<int>(std::ranges::count_if(m_slots, [&](const Slot& slot) {
        return slot.free && scene && scene->find(slot.root);
    }));
}

int PrefabPool::total() const
{
    const Scene* scene = owner() ? owner()->scene() : nullptr;

    return static_cast<int>(std::ranges::count_if(m_slots, [&](const Slot& slot) {
        return scene && scene->find(slot.root);
    }));
}

} // namespace yqlora
