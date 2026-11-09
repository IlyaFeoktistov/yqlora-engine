#include "yqlora/scene/TreeChangeQueue.hpp"

#include "yqlora/core/Log.hpp"
#include "yqlora/scene/NodeTree.hpp"
#include "yqlora/scene/Scene.hpp"

#include <algorithm>
#include <cassert>
#include <memory>
#include <utility>

namespace yqlora {

void TreeChangeQueue::destroy(const Node& node)
{
    m_changes.push_back({.node = node.id()});
}

void TreeChangeQueue::destroy(const Component& component)
{
    m_changes.push_back({.node = component.owner()->id(), .component = &component});
}

void TreeChangeQueue::setParent(const Node& node, const Node& parent, KeepWorld keepWorld)
{
    m_changes.push_back({.node = node.id(), .parent = parent.id(), .keepWorld = keepWorld});
}

void TreeChangeQueue::apply(Scene& scene)
{
    assert(!scene.inFrame() && "TreeChangeQueue::apply: the frame is not over");
    for (const Change& change : std::exchange(m_changes, {})) {
        Node* node = scene.find(change.node);
        if (!node) {
            // Ушёл раньше вместе с уничтоженной веткой.
            continue;
        }
        if (change.component) {
            const auto components = node->components();
            const auto found = std::ranges::find(components, change.component, &std::unique_ptr<Component>::get);
            // Уничтожен раньше, второй раз ничего не делает.
            if (found != components.end()) {
                scene.destroy(**found);
            }
            continue;
        }
        if (change.parent == 0) {
            scene.destroy(*node);
            continue;
        }
        Node* parent = scene.find(change.parent);
        // Прошлые изменения кадра могли сделать перенос невозможным.
        if (!parent || !node->parent() || isInBranch(*node, *parent)) {
            log::error("Scene: node '{}' cannot be moved, the tree has changed during the frame", node->name());
            continue;
        }
        scene.setParent(*node, *parent, change.keepWorld);
    }
}

} // namespace yqlora
