#pragma once

#include "yqlora/math/Math.hpp"

#include <variant>

namespace yqlora {

// Фигура коллайдера в его собственных осях, с центром в начале координат:
// что коллайдер описывает, без типов физической библиотеки. Форму из неё
// строит физика движка. Оси капсулы и цилиндра - по Y.
struct BoxShape {
    Vec3 halfExtents{0.5f};
};

struct SphereShape {
    float radius = 0.5f;
};

// halfHeight - половина прямой части, без полусфер.
struct CapsuleShape {
    float radius = 0.5f;
    float halfHeight = 0.5f;
};

struct CylinderShape {
    float radius = 0.5f;
    float halfHeight = 1.0f;
};

using ColliderShape = std::variant<BoxShape, SphereShape, CapsuleShape, CylinderShape>;

} // namespace yqlora
