#pragma once

#include "yqlora/input/Input.hpp"
#include "yqlora/math/Math.hpp"
#include "yqlora/math/Ray.hpp"
#include "yqlora/render/FrameData.hpp"

namespace yqlora::editor {

// Камера редактора. Живёт вне сцены: сцена не меняется оттого, что на неё смотрят.
//
// Пока зажата правая кнопка мыши: мышь поворачивает камеру, WASD двигает,
// Q и E опускают и поднимают, Shift ускоряет. Колесо мыши меняет скорость.
class EditorCamera {
public:
    // hovered: курсор над окном сцены. Полёт начинается, только если правую
    // кнопку нажали над ним, и длится, пока её не отпустят, даже если курсор ушёл.
    void update(const Input& input, float deltaSeconds, bool hovered);

    // Правая кнопка зажата: курсор стоит захватить, а клики не считать выбором.
    bool flying() const
    {
        return m_flying;
    }

    const Vec3& position() const
    {
        return m_position;
    }

    void setPosition(const Vec3& position)
    {
        m_position = position;
    }

    void lookAt(const Vec3& target);
    // Смотреть вдоль направления, не сдвигаясь с места.
    void lookAlong(const Vec3& direction);

    // Метров в секунду.
    float speed() const
    {
        return m_speed;
    }

    void setSpeed(float speed);

    Vec3 forward() const;
    Vec3 right() const;
    Vec3 up() const;

    CameraData cameraData(float aspect) const;

    // Луч из камеры через точку окна. point и viewportSize в одних единицах,
    // отсчёт от левого верхнего угла, как у положения мыши.
    Ray screenRay(const Vec2& point, const Vec2& viewportSize) const;

private:
    Vec3 m_position{0.0f, 2.0f, 5.0f};
    // Поворот вокруг вертикали и наклон. При нулях камера смотрит вдоль -Z.
    float m_yawDegrees = 0.0f;
    float m_pitchDegrees = -20.0f;
    float m_speed = 5.0f;
    float m_fovDegrees = 60.0f;
    float m_near = 0.05f;
    float m_far = 1000.0f;
    bool m_flying = false;
};

} // namespace yqlora::editor
