#include "yqlora/editor/JointEditing.hpp"

#include "yqlora/physics/BodyPose.hpp"
#include "yqlora/physics/Joint.hpp"
#include "yqlora/scene/Node3D.hpp"
#include "yqlora/scene/Scene.hpp"

#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>

namespace yqlora::editor {

JointWorldFrame jointWorldFrame(const Joint& joint, const Node3D& node)
{
    // Как физика: точка - через всю матрицу узла, оси - только поворотом.
    const Mat4& world = node.worldMatrix();
    const Quat rotation = bodyPoseOf(world).rotation;
    const Vec3 axis = jointAxis(joint.definition());

    return {
        .point = Vec3(world * Vec4(joint.anchor(), 1.0f)),
        .axis = glm::normalize(rotation * axis),
        .normal = glm::normalize(rotation * jointNormal(axis)),
    };
}

std::vector<Vec3> jointArc(const JointWorldFrame& frame, float fromDegrees, float toDegrees, float radius, int segments)
{
    const int count = std::max(segments, 1);
    std::vector<Vec3> points;
    points.reserve(static_cast<std::size_t>(count) + 1);
    for (int i = 0; i <= count; ++i) {
        const float degrees = fromDegrees
                              + (toDegrees - fromDegrees) * static_cast<float>(i) / static_cast<float>(count);
        const Quat turn = glm::angleAxis(glm::radians(degrees), frame.axis);
        points.push_back(frame.point + turn * frame.normal * radius);
    }

    return points;
}

std::optional<Vec3> connectedPoint(const Joint& joint, const Scene& scene)
{
    const auto* other = scene.find<Node3D>(joint.connectedBody());
    if (const auto* distance = dynamic_cast<const DistanceJoint*>(&joint)) {
        const Vec3& anchor = distance->connectedAnchor();

        return other ? Vec3(other->worldMatrix() * Vec4(anchor, 1.0f)) : anchor;
    }
    if (!other) {
        return std::nullopt;
    }

    return other->worldPosition();
}

} // namespace yqlora::editor
