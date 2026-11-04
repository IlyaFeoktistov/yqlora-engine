#include "yqlora/render/Camera3D.hpp"

#include "yqlora/render/RenderSystem.hpp"
#include "yqlora/scene/Scene.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cassert>
#include <cmath>

namespace yqlora {

namespace {

// При 0 и 180 градусах проекция вырождается.
constexpr float kMinFovDegrees = 1.0f;
constexpr float kMaxFovDegrees = 179.0f;

constexpr float kMinNearPlane = 0.001f;
constexpr float kMinPlaneGap = 0.001f;

// Насыщение датчика ISO 100 относительно средней яркости: 78 / 65.
constexpr float kSaturationMargin = 1.2f;

} // namespace

float exposureFactor(float ev100)
{
    return 1.0f / (kSaturationMargin * std::exp2(ev100));
}

Camera3D::Camera3D(std::string name)
    : Node3D(std::move(name))
{
}

void Camera3D::onEnterScene()
{
    Node3D::onEnterScene();
    if (RenderSystem* render = scene()->system<RenderSystem>()) {
        render->addCamera(*this);
    }
}

void Camera3D::onExitScene()
{
    if (RenderSystem* render = scene()->system<RenderSystem>()) {
        render->removeCamera(*this);
    }
    Node3D::onExitScene();
}

void Camera3D::setFovDegrees(float fov)
{
    m_fovDegrees = std::clamp(fov, kMinFovDegrees, kMaxFovDegrees);
}

void Camera3D::setClipPlanes(float nearPlane, float farPlane)
{
    assert(nearPlane > 0.0f && farPlane > nearPlane);
    m_near = nearPlane;
    m_far = farPlane;
}

void Camera3D::setNearPlane(float nearPlane)
{
    m_near = std::max(nearPlane, kMinNearPlane);
    m_far = std::max(m_far, m_near + kMinPlaneGap);
}

void Camera3D::setFarPlane(float farPlane)
{
    m_far = std::max(farPlane, m_near + kMinPlaneGap);
}

Mat4 Camera3D::viewMatrix() const
{
    // View это обратное преобразование к положению камеры в мире.
    return glm::inverse(worldMatrix());
}

Mat4 Camera3D::projectionMatrix(float aspect) const
{
    return math::perspective(glm::radians(m_fovDegrees), aspect, m_near, m_far);
}

} // namespace yqlora
