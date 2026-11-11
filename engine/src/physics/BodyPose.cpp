#include "yqlora/physics/BodyPose.hpp"

#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

namespace yqlora {

namespace {

Vec3 axisScale(const Mat4& world)
{
    return {glm::length(Vec3(world[0])), glm::length(Vec3(world[1])), glm::length(Vec3(world[2]))};
}

} // namespace

BodyPose bodyPoseOf(const Mat4& world)
{
    const Vec3 scale = axisScale(world);
    glm::mat3 basis(Vec3(world[0]) / scale.x, Vec3(world[1]) / scale.y, Vec3(world[2]) / scale.z);

    return {Vec3(world[3]), glm::normalize(glm::quat_cast(basis))};
}

Mat4 worldWithPose(const Mat4& world, const BodyPose& pose)
{
    return glm::translate(Mat4(1.0f), pose.position) * glm::mat4_cast(pose.rotation)
           * glm::scale(Mat4(1.0f), axisScale(world));
}

BodyPose blendPoses(const BodyPose& a, const BodyPose& b, float t)
{
    return {glm::mix(a.position, b.position, t), glm::slerp(a.rotation, b.rotation, t)};
}

} // namespace yqlora
