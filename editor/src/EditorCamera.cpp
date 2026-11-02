#include "yqlora/editor/EditorCamera.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace yqlora::editor {

namespace {

constexpr float kMouseSensitivity = 0.15f; // градусов на точку сдвига мыши
constexpr float kBoostFactor = 3.0f;       // с зажатым Shift
constexpr float kWheelStep = 1.2f;         // во сколько раз меняет скорость один щелчок колеса
constexpr float kMinSpeed = 0.1f;
constexpr float kMaxSpeed = 500.0f;
// Не даём смотреть ровно вверх или вниз: там направление "вправо" не определено.
constexpr float kMaxPitch = 89.0f;

float axis(const Input& input, Key negative, Key positive)
{
    return (input.isKeyDown(positive) ? 1.0f : 0.0f) - (input.isKeyDown(negative) ? 1.0f : 0.0f);
}

} // namespace

void EditorCamera::update(const Input& input, float deltaSeconds, bool hovered)
{
    if (!input.isMouseButtonDown(MouseButton::Right)) {
        m_flying = false;
    } else if (hovered && input.wasMouseButtonPressed(MouseButton::Right)) {
        m_flying = true;
    }

    if ((hovered || m_flying) && input.wheelDelta() != 0.0f) {
        setSpeed(m_speed * std::pow(kWheelStep, input.wheelDelta()));
    }
    if (!m_flying) {
        return;
    }

    // Мышь вправо - поворот вправо, мышь вниз - взгляд вниз.
    const Vec2 delta = input.mouseDelta();
    m_yawDegrees -= delta.x * kMouseSensitivity;
    m_pitchDegrees = std::clamp(m_pitchDegrees - delta.y * kMouseSensitivity, -kMaxPitch, kMaxPitch);

    const Vec3 direction = forward() * axis(input, Key::S, Key::W) + right() * axis(input, Key::A, Key::D)
                           + math::kWorldUp * axis(input, Key::Q, Key::E);
    if (glm::length(direction) == 0.0f) {
        return;
    }
    const bool boost = input.isKeyDown(Key::LeftShift) || input.isKeyDown(Key::RightShift);
    const float distance = m_speed * (boost ? kBoostFactor : 1.0f) * deltaSeconds;
    m_position += glm::normalize(direction) * distance;
}

void EditorCamera::lookAt(const Vec3& target)
{
    lookAlong(target - m_position);
}

void EditorCamera::lookAlong(const Vec3& direction)
{
    if (glm::length(direction) < 1e-6f) {
        return;
    }
    const Vec3 d = glm::normalize(direction);
    m_pitchDegrees = std::clamp(glm::degrees(std::asin(d.y)), -kMaxPitch, kMaxPitch);
    // Ровно вверх или вниз: направление по горизонтали не определено, поворот вокруг вертикали оставляем.
    if (std::abs(d.x) > 1e-6f || std::abs(d.z) > 1e-6f) {
        m_yawDegrees = glm::degrees(std::atan2(-d.x, -d.z));
    }
}

void EditorCamera::setSpeed(float speed)
{
    m_speed = std::clamp(speed, kMinSpeed, kMaxSpeed);
}

Vec3 EditorCamera::forward() const
{
    const float yaw = glm::radians(m_yawDegrees);
    const float pitch = glm::radians(m_pitchDegrees);

    return {-std::sin(yaw) * std::cos(pitch), std::sin(pitch), -std::cos(yaw) * std::cos(pitch)};
}

Vec3 EditorCamera::right() const
{
    return glm::normalize(glm::cross(forward(), math::kWorldUp));
}

Vec3 EditorCamera::up() const
{
    return glm::cross(right(), forward());
}

CameraData EditorCamera::cameraData(float aspect) const
{
    CameraData camera;
    camera.view = glm::lookAt(m_position, m_position + forward(), math::kWorldUp);
    camera.projection = math::perspective(glm::radians(m_fovDegrees), aspect, m_near, m_far);
    camera.position = m_position;

    return camera;
}

Ray EditorCamera::screenRay(const Vec2& point, const Vec2& viewportSize) const
{
    // Точка экрана в диапазоне [-1, 1], ось Y вверх.
    const float x = 2.0f * point.x / viewportSize.x - 1.0f;
    const float y = 1.0f - 2.0f * point.y / viewportSize.y;
    const float tanHalfFov = std::tan(glm::radians(m_fovDegrees) * 0.5f);
    const float aspect = viewportSize.x / viewportSize.y;

    const Vec3 direction = forward() + right() * (x * tanHalfFov * aspect) + up() * (y * tanHalfFov);

    return {m_position, glm::normalize(direction)};
}

} // namespace yqlora::editor
