#include "yqlora/scripting/Script.hpp"

#include "yqlora/core/EngineServices.hpp"
#include "yqlora/scene/Node3D.hpp"
#include "yqlora/scene/NodeTree.hpp"
#include "yqlora/scene/Scene.hpp"

#include <cassert>

namespace yqlora {

Node& Script::node() const
{
    assert(owner() && "Script::node: the script is not on a node");

    return *owner();
}

Scene& Script::scene() const
{
    Scene* scene = node().scene();
    assert(scene && "Script::scene: the node is not in a scene");

    return *scene;
}

Transform3D& Script::transform() const
{
    auto* spatial = dynamic_cast<Node3D*>(&node());
    assert(spatial && "Script::transform: the node of the script is not a Node3D");

    return spatial->transform();
}

const Input& Script::input() const
{
    return scene().services().input();
}

const GameTime& Script::time() const
{
    return scene().time();
}

Timers Script::timers()
{
    TimerSystem* system = scene().system<TimerSystem>();
    assert(system && "timers: TimerSystem is registered with the engine types");

    return Timers(*system, *this);
}

Node* Script::findNode(std::string_view name) const
{
    Node* root = scene().root();

    return root ? findNodeByName(*root, name) : nullptr;
}

} // namespace yqlora
