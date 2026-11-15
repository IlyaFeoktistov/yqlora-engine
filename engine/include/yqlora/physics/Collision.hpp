#pragma once

#include "yqlora/math/Math.hpp"
#include "yqlora/reflection/Property.hpp"
#include "yqlora/reflection/Record.hpp"

#include <vector>

namespace yqlora {

class Collider;

// Столкновение одного коллайдера тела с коллайдером другого тела: запись
// событий RigidBody::onCollisionEnter, onCollisionStay и onCollisionExit.
// Всё - с точки зрения тела, которое получает событие.
struct Collision {
    // Узел другого тела (с RigidBody или с коллайдерами без него).
    NodeRef other;
    // Свой коллайдер (тело собирают и коллайдеры потомков) и коллайдер другого.
    ComponentRef<Collider> collider;
    ComponentRef<Collider> otherCollider;
    // Точки контакта в мире; у выхода - пусто.
    std::vector<Vec3> points;
    // Единичная нормаль контакта: куда другое тело толкает это.
    Vec3 normal{0.0f};
    // Скорость этого тела относительно другого в точке контакта, м/с.
    Vec3 relativeVelocity{0.0f};
    // Импульс удара, Н*с: (1 + e) на скорость сближения по нормали и
    // приведённую массу пары, без вращения; у тела без массы (неподвижного,
    // кинематического) масса бесконечна. Для силы удара в звуке и уроне; у
    // продолжения и выхода - 0.
    float impulse = 0.0f;

    bool operator==(const Collision&) const = default;
};

} // namespace yqlora

YQLORA_RECORD(
    yqlora::Collision,
    field("other", &yqlora::Collision::other),
    field("collider", &yqlora::Collision::collider),
    field("otherCollider", &yqlora::Collision::otherCollider),
    field("points", &yqlora::Collision::points),
    field("normal", &yqlora::Collision::normal),
    field("relativeVelocity", &yqlora::Collision::relativeVelocity),
    field("impulse", &yqlora::Collision::impulse)
)
