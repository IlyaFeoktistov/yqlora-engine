#include "yqlora/editor/ColliderEditing.hpp"

#include "yqlora/physics/ConvexCollider.hpp"
#include "yqlora/physics/MeshCollider.hpp"
#include "yqlora/physics/RigidBody.hpp"
#include "yqlora/render/MeshRenderer.hpp"
#include "yqlora/resource/ResourceCache.hpp"
#include "yqlora/scene/Node3D.hpp"

#include <glm/common.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include <algorithm>
#include <variant>

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
    HandleShape handles{.center = collider.center(), .rotation = collider.rotation()};
    // Ручки стоят на поверхности фигуры: у капсулы высота - с полусферами.
    const ColliderShape shape = collider.shape();
    if (const auto* box = std::get_if<BoxShape>(&shape)) {
        handles.halfExtents = box->halfExtents;
        handles.links = ShapeLinks::None;
    } else if (const auto* sphere = std::get_if<SphereShape>(&shape)) {
        handles.halfExtents = Vec3(sphere->radius);
        handles.links = ShapeLinks::All;
    } else if (const auto* capsule = std::get_if<CapsuleShape>(&shape)) {
        handles.halfExtents = Vec3(capsule->radius, capsule->halfHeight + capsule->radius, capsule->radius);
        handles.links = ShapeLinks::Radial;
    } else if (const auto* cylinder = std::get_if<CylinderShape>(&shape)) {
        handles.halfExtents = Vec3(cylinder->radius, cylinder->halfHeight, cylinder->radius);
        handles.links = ShapeLinks::Radial;
    } else {
        return std::nullopt;
    }

    return handles;
}

void applyHandleShape(Collider& collider, const HandleShape& handles)
{
    const Vec3 half = handles.halfExtents;
    ColliderShape shape = collider.shape();
    if (auto* box = std::get_if<BoxShape>(&shape)) {
        box->halfExtents = half;
    } else if (auto* sphere = std::get_if<SphereShape>(&shape)) {
        sphere->radius = half.x;
    } else if (auto* capsule = std::get_if<CapsuleShape>(&shape)) {
        *capsule = {.radius = half.x, .halfHeight = half.y - half.x};
    } else if (auto* cylinder = std::get_if<CylinderShape>(&shape)) {
        *cylinder = {.radius = half.x, .halfHeight = half.y};
    } else {
        return;
    }
    collider.setShape(shape);
    collider.setCenter(handles.center);
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
    if (std::holds_alternative<MeshShape>(collider.shape())) {
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
