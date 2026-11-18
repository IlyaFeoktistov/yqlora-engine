#include "yqlora/editor/ColliderEditing.hpp"

#include "yqlora/physics/BoxCollider.hpp"
#include "yqlora/physics/CapsuleCollider.hpp"
#include "yqlora/physics/ConvexCollider.hpp"
#include "yqlora/physics/CylinderCollider.hpp"
#include "yqlora/physics/MeshCollider.hpp"
#include "yqlora/physics/RigidBody.hpp"
#include "yqlora/physics/SphereCollider.hpp"
#include "yqlora/render/MeshRenderer.hpp"
#include "yqlora/resource/ResourceCache.hpp"
#include "yqlora/scene/Node3D.hpp"

#include <glm/common.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include <algorithm>

namespace yqlora::editor {

namespace {

constexpr int kBoundsCorners = 8;

// Обходит узел и его потомков, не заходя в детей со своим RigidBody.
template <typename Visit>
void forEachInBody(const Node& node, Visit&& visit)
{
    visit(node);
    for (const auto& child : node.children()) {
        if (!child->getComponent<RigidBody>()) {
            forEachInBody(*child, visit);
        }
    }
}

void include(Bounds& bounds, const Vec3& point, bool& empty)
{
    if (empty) {
        bounds = {point, point};
        empty = false;

        return;
    }
    bounds.min = glm::min(bounds.min, point);
    bounds.max = glm::max(bounds.max, point);
}

// Меш, который рисует рендерер узла; 0 - рендерера или меша нет.
ResourceRef rendererMesh(const Node& node)
{
    const auto* renderer = node.getComponent<MeshRenderer>();

    return renderer ? renderer->mesh() : ResourceRef{};
}

// Коллайдер-меш: меш, который он берёт у рендерера при подгонке.
template <typename MeshShapeCollider>
bool fitMesh(MeshShapeCollider& collider, const Node3D& node)
{
    const ResourceRef mesh = rendererMesh(node);
    if (mesh.id == 0) {
        return false;
    }
    collider.setMesh(mesh);

    return true;
}

} // namespace

std::optional<HandleShape> colliderHandleShape(const Collider& collider)
{
    HandleShape shape{.center = collider.center(), .rotation = collider.rotation()};
    if (const auto* box = dynamic_cast<const BoxCollider*>(&collider)) {
        shape.halfExtents = box->size() * 0.5f;
        shape.links = ShapeLinks::None;
    } else if (const auto* sphere = dynamic_cast<const SphereCollider*>(&collider)) {
        shape.halfExtents = Vec3(sphere->radius());
        shape.links = ShapeLinks::All;
    } else if (const auto* capsule = dynamic_cast<const CapsuleCollider*>(&collider)) {
        shape.halfExtents = Vec3(capsule->radius(), capsule->height() * 0.5f, capsule->radius());
        shape.links = ShapeLinks::Radial;
    } else if (const auto* cylinder = dynamic_cast<const CylinderCollider*>(&collider)) {
        shape.halfExtents = Vec3(cylinder->radius(), cylinder->height() * 0.5f, cylinder->radius());
        shape.links = ShapeLinks::Radial;
    } else {
        return std::nullopt;
    }

    return shape;
}

void applyHandleShape(Collider& collider, const HandleShape& shape)
{
    if (auto* box = dynamic_cast<BoxCollider*>(&collider)) {
        box->setSize(shape.halfExtents * 2.0f);
    } else if (auto* sphere = dynamic_cast<SphereCollider*>(&collider)) {
        sphere->setRadius(shape.halfExtents.x);
    } else if (auto* capsule = dynamic_cast<CapsuleCollider*>(&collider)) {
        capsule->setRadius(shape.halfExtents.x);
        capsule->setHeight(shape.halfExtents.y * 2.0f);
    } else if (auto* cylinder = dynamic_cast<CylinderCollider*>(&collider)) {
        cylinder->setRadius(shape.halfExtents.x);
        cylinder->setHeight(shape.halfExtents.y * 2.0f);
    } else {
        return;
    }
    collider.setCenter(shape.center);
}

Mat4 colliderWorldMatrix(const Collider& collider, const Node3D& node)
{
    return node.worldMatrix() * glm::translate(Mat4(1.0f), collider.center()) * glm::mat4_cast(collider.rotation());
}

std::optional<Bounds> branchMeshBounds(const Node3D& node, ResourceCache& resources)
{
    const Mat4 toNode = glm::inverse(node.worldMatrix());
    Bounds bounds;
    bool empty = true;
    forEachInBody(node, [&](const Node& visited) {
        const auto* spatial = dynamic_cast<const Node3D*>(&visited);
        const ResourceRef mesh = rendererMesh(visited);
        const MeshData* data = spatial && mesh.id != 0 ? resources.find<MeshData>(mesh.id) : nullptr;
        if (!data || data->vertices.empty()) {
            return;
        }
        const Mat4 toLocal = toNode * spatial->worldMatrix();
        for (int corner = 0; corner < kBoundsCorners; ++corner) {
            const Vec3 point(
                (corner & 1) != 0 ? data->bounds.max.x : data->bounds.min.x,
                (corner & 2) != 0 ? data->bounds.max.y : data->bounds.min.y,
                (corner & 4) != 0 ? data->bounds.max.z : data->bounds.min.z
            );
            include(bounds, Vec3(toLocal * Vec4(point, 1.0f)), empty);
        }
    });
    if (empty) {
        return std::nullopt;
    }

    return bounds;
}

bool fitCollider(Collider& collider, const Node3D& node, ResourceCache& resources)
{
    if (auto* mesh = dynamic_cast<MeshCollider*>(&collider)) {
        return fitMesh(*mesh, node);
    }
    if (auto* convex = dynamic_cast<ConvexCollider*>(&collider)) {
        return fitMesh(*convex, node);
    }
    const auto bounds = branchMeshBounds(node, resources);
    auto shape = colliderHandleShape(collider);
    if (!bounds || !shape) {
        return false;
    }

    const Vec3 half = (bounds->max - bounds->min) * 0.5f;
    shape->center = (bounds->max + bounds->min) * 0.5f;
    switch (shape->links) {
    case ShapeLinks::None: shape->halfExtents = half; break;
    case ShapeLinks::All: shape->halfExtents = Vec3(std::max({half.x, half.y, half.z})); break;
    case ShapeLinks::Radial: {
        const float radius = std::max(half.x, half.z);
        shape->halfExtents = Vec3(radius, half.y, radius);
        break;
    }
    }
    collider.setRotationDegrees(Vec3(0.0f));
    applyHandleShape(collider, *shape);

    return true;
}

bool canFitCollider(const Collider& collider, const Node3D& node, ResourceCache& resources)
{
    if (dynamic_cast<const MeshCollider*>(&collider) || dynamic_cast<const ConvexCollider*>(&collider)) {
        return rendererMesh(node).id != 0;
    }

    return colliderHandleShape(collider) && branchMeshBounds(node, resources);
}

std::vector<const Collider*> bodyColliders(const Node& node)
{
    const Node* root = &node;
    for (const Node* up = &node; up; up = up->parent()) {
        if (up->getComponent<RigidBody>()) {
            root = up;
            break;
        }
    }

    std::vector<const Collider*> colliders;
    forEachInBody(*root, [&](const Node& visited) {
        for (const Collider* collider : visited.getComponents<Collider>()) {
            colliders.push_back(collider);
        }
    });

    return colliders;
}

} // namespace yqlora::editor
