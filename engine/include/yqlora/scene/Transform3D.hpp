#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/math/Math.hpp"
#include "yqlora/scene/Component.hpp"

#include <cstdint>

namespace yqlora {

// Положение, поворот и масштаб относительно родителя разом, как их видно в
// инспекторе: поворот углами Эйлера в градусах.
struct TransformValues {
    Vec3 position{0.0f};
    Vec3 rotationDegrees{0.0f};
    Vec3 scale{1.0f};

    bool operator==(const TransformValues&) const = default;
};

// Положение, поворот и масштаб узла Node3D относительно родителя.
// Встроенный компонент: создаётся вместе с Node3D и снять его нельзя.
// Мировую матрицу из него собирает узел, сам Transform про дерево не знает.
//
// Система координат правая: +X вправо, +Y вверх, взгляд вдоль -Z.
class YQLORA_API Transform3D final : public Component {
public:
    YQLORA_TYPE(Transform3D, Component)

    const Vec3& position() const
    {
        return m_position;
    }

    const Quat& rotation() const
    {
        return m_rotation;
    }

    const Vec3& scale() const
    {
        return m_scale;
    }

    void setPosition(const Vec3& position);
    void setRotation(const Quat& rotation);
    void setScale(const Vec3& scale);

    // Углы Эйлера в градусах (pitch по X, yaw по Y, roll по Z).
    // Возвращаются ровно такими, как заданы: из кватерниона углы восстанавливаются
    // неоднозначно (yaw 100 градусов превращается в (180, 80, 180)), и поле в инспекторе прыгало бы.
    // После setRotation/lookAt углы вычисляются из кватерниона.
    void setRotationDegrees(const Vec3& eulerDegrees);
    const Vec3& rotationDegrees() const
    {
        return m_rotationDegrees;
    }

    TransformValues values() const
    {
        return {m_position, m_rotationDegrees, m_scale};
    }

    void setValues(const TransformValues& values);

    // Поворачивает так, чтобы -Z смотрел на target (в координатах родителя).
    void lookAt(const Vec3& target, const Vec3& up = math::kWorldUp);

    // Матрица T * R * S в пространстве родителя.
    Mat4 matrix() const;

    // Растёт с каждым изменением значений: по ней подсистемы узнают, что
    // узел сдвинулся относительно своего родителя, не сравнивая матрицы.
    std::uint64_t revision() const
    {
        return m_revision;
    }

private:
    friend class Node3D;
    Transform3D() = default;

    // Значения изменились: мировая матрица узла и его ветки устарела.
    void touch();

    Vec3 m_position{0.0f};
    Quat m_rotation{1.0f, 0.0f, 0.0f, 0.0f};
    Vec3 m_rotationDegrees{0.0f};
    Vec3 m_scale{1.0f};
    std::uint64_t m_revision = 0;
};

} // namespace yqlora
