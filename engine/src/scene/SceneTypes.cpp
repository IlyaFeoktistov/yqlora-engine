#include "yqlora/scene/SceneTypes.hpp"

#include "yqlora/reflection/TypeRegistry.hpp"
#include "yqlora/scene/ExecutionOrderSettings.hpp"
#include "yqlora/scene/LayerSettings.hpp"
#include "yqlora/scene/Node3D.hpp"
#include "yqlora/scene/TimeSettings.hpp"
#include "yqlora/scene/TimerSystem.hpp"
#include "yqlora/scene/Transform3D.hpp"
#include "yqlora/scene/TransformSystem3D.hpp"

namespace yqlora {

void registerSceneTypes(TypeRegistry& registry)
{
    registry.registerNode<Node>()
        .property("name", &Node::name, &Node::setName)
        .property("active", &Node::active, &Node::setActive)
        .property("layer", &Node::layer, &Node::setLayer, PropertyHint::Layer)
        .property("groups", &Node::groups, &Node::setGroups)
        .method("findChild", &Node::findChild, {{"name"}})
        .method("find", &Node::find, {{"path"}})
        .method("isInGroup", &Node::isInGroup, {{"group"}});

    registry.registerComponent<Component>().property("enabled", &Component::enabled, &Component::setEnabled);

    registry.registerNode<Node3D>()
        .method("worldPosition", &Node3D::worldPosition)
        .method("setWorldPosition", &Node3D::setWorldPosition, {{"position"}})
        .method("toWorld", &Node3D::toWorld, {{"localPoint"}})
        .method("toLocal", &Node3D::toLocal, {{"worldPoint"}})
        .method("worldForward", &Node3D::worldForward)
        .method("worldRight", &Node3D::worldRight)
        .method("worldUp", &Node3D::worldUp);

    registry.registerComponent<Transform3D>()
        .property("position", &Transform3D::position, &Transform3D::setPosition)
        .property("rotation", &Transform3D::rotationDegrees, &Transform3D::setRotationDegrees, PropertyHint::Angle)
        .property("scale", &Transform3D::scale, &Transform3D::setScale)
        .method("lookAt", &Transform3D::lookAt, {{"target"}, {"up", math::kWorldUp}});

    registry.registerSystem<SceneSystem>();
    registry.registerSystem<TransformSystem3D>();
    registry.registerSystem<TimerSystem>();

    registry.registerSettings<ExecutionOrderSettings>("execution_order")
        .property("entries", &ExecutionOrderSettings::entries);

    registry.registerSettings<LayerSettings>("layers")
        .property("names", &LayerSettings::names, &LayerSettings::setNames, PropertyHint::LayerNames);

    registry.registerSettings<TimeSettings>("time")
        .property("fixedStep", &TimeSettings::fixedStep, &TimeSettings::setFixedStep)
        .property("maxFrame", &TimeSettings::maxFrame, &TimeSettings::setMaxFrame);
}

} // namespace yqlora
