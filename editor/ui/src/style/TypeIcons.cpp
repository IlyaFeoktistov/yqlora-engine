#include "yqlora/editor/ui/style/TypeIcons.hpp"

#include "yqlora/editor/ui/style/Icons.hpp"

#include "yqlora/physics/BoxCollider.hpp"
#include "yqlora/physics/CapsuleCollider.hpp"
#include "yqlora/physics/CharacterController.hpp"
#include "yqlora/physics/ConvexCollider.hpp"
#include "yqlora/physics/CylinderCollider.hpp"
#include "yqlora/physics/Joint.hpp"
#include "yqlora/physics/MeshCollider.hpp"
#include "yqlora/physics/PhysicsChain.hpp"
#include "yqlora/physics/RigidBody.hpp"
#include "yqlora/physics/SphereCollider.hpp"
#include "yqlora/reflection/TypeRegistry.hpp"
#include "yqlora/render/Camera3D.hpp"
#include "yqlora/render/DirectionalLight3D.hpp"
#include "yqlora/render/Environment.hpp"
#include "yqlora/render/MeshRenderer.hpp"
#include "yqlora/render/PointLight3D.hpp"
#include "yqlora/render/SpotLight3D.hpp"
#include "yqlora/scene/Node3D.hpp"
#include "yqlora/scene/PrefabPool.hpp"
#include "yqlora/scene/Timer.hpp"
#include "yqlora/scripting/Script.hpp"

#include <algorithm>
#include <iterator>
#include <utility>

namespace yqlora::editor {

namespace {

using IconRow = std::pair<std::string_view, const char*>;

const IconRow kIcons[] = {
    {Node::staticTypeName(), icons::kNode},
    {Node3D::staticTypeName(), icons::kNode3D},
    {Camera3D::staticTypeName(), icons::kCamera3D},
    {DirectionalLight3D::staticTypeName(), icons::kDirectionalLight3D},
    {PointLight3D::staticTypeName(), icons::kPointLight3D},
    {SpotLight3D::staticTypeName(), icons::kSpotLight3D},
    {Environment::staticTypeName(), icons::kEnvironment},
    {Component::staticTypeName(), icons::kComponent},
    {Timer::staticTypeName(), icons::kTimer},
    {PrefabPool::staticTypeName(), icons::kPrefabPool},
    {MeshRenderer::staticTypeName(), icons::kMeshRenderer},
    {RigidBody::staticTypeName(), icons::kRigidBody},
    {Collider::staticTypeName(), icons::kCollider},
    {BoxCollider::staticTypeName(), icons::kBoxCollider},
    {SphereCollider::staticTypeName(), icons::kSphereCollider},
    {CapsuleCollider::staticTypeName(), icons::kCapsuleCollider},
    {CylinderCollider::staticTypeName(), icons::kCylinderCollider},
    {ConvexCollider::staticTypeName(), icons::kConvexCollider},
    {MeshCollider::staticTypeName(), icons::kMeshCollider},
    {CharacterController::staticTypeName(), icons::kCharacter},
    {Joint::staticTypeName(), icons::kJoint},
    {FixedJoint::staticTypeName(), icons::kFixedJoint},
    {HingeJoint::staticTypeName(), icons::kHingeJoint},
    {SliderJoint::staticTypeName(), icons::kSliderJoint},
    {DistanceJoint::staticTypeName(), icons::kDistanceJoint},
    {ConeJoint::staticTypeName(), icons::kConeJoint},
    {PhysicsChain::staticTypeName(), icons::kPhysicsChain},
    {Script::staticTypeName(), icons::kScript},
};

const char* tableIcon(std::string_view type)
{
    const auto found = std::ranges::find(kIcons, type, &IconRow::first);

    return found != std::end(kIcons) ? found->second : nullptr;
}

} // namespace

const char* findTypeIcon(const TypeRegistry& registry, const TypeInfo& type)
{
    for (const TypeInfo* t : registry.chain(type)) {
        // Значок корня у наследника - не его значок, а значок по умолчанию.
        if (t->baseName().empty() && t != &type) {
            return nullptr;
        }
        if (const char* icon = tableIcon(t->name())) {
            return icon;
        }
    }

    return nullptr;
}

const char* typeIcon(const TypeRegistry& registry, const TypeInfo& type)
{
    if (const char* icon = findTypeIcon(registry, type)) {
        return icon;
    }

    return type.kind() == TypeKind::Component ? icons::kComponent : icons::kNode;
}

const char* typeIcon(std::string_view type)
{
    const char* icon = tableIcon(type);

    return icon ? icon : icons::kNode;
}

} // namespace yqlora::editor
