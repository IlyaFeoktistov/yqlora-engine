#include "yqlora/scene/Transform3D.hpp"

#include "yqlora/scene/Node3D.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace yqlora {

void Transform3D::setPosition(const Vec3& position)
{
    m_position = position;
    touch();
}

void Transform3D::setRotation(const Quat& rotation)
{
    m_rotation = glm::normalize(rotation);
    m_rotationDegrees = glm::degrees(glm::eulerAngles(m_rotation));
    touch();
}

void Transform3D::setScale(const Vec3& scale)
{
    m_scale = scale;
    touch();
}

void Transform3D::setRotationDegrees(const Vec3& eulerDegrees)
{
    m_rotation = glm::normalize(Quat(glm::radians(eulerDegrees)));
    m_rotationDegrees = eulerDegrees;
    touch();
}

void Transform3D::setValues(const TransformValues& values)
{
    setPosition(values.position);
    setRotationDegrees(values.rotationDegrees);
    setScale(values.scale);
}

void Transform3D::lookAt(const Vec3& target, const Vec3& up)
{
    const Vec3 direction = target - m_position;
    if (glm::length(direction) < 1e-6f) {
        return;
    }
    setRotation(glm::quatLookAt(glm::normalize(direction), up));
}

void Transform3D::touch()
{
    ++m_revision;
    // Встроенный компонент живёт только на Node3D; до подключения узла нет.
    if (Node* node = owner()) {
        static_cast<Node3D*>(node)->markWorldDirty();
    }
}

Mat4 Transform3D::matrix() const
{
    Mat4 m = glm::translate(Mat4(1.0f), m_position);
    m *= glm::toMat4(m_rotation);
    m = glm::scale(m, m_scale);

    return m;
}

} // namespace yqlora
