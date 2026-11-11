#pragma once

// Только для кода движка, который включает Jolt (Jolt.h - раньше этого файла).

#include "yqlora/math/Math.hpp"
#include "yqlora/physics/ColliderShape.hpp"

#include <Jolt/Physics/Collision/Shape/Shape.h>

#include <span>

namespace yqlora {

// Фигура коллайдера на своём месте в осях тела.
struct PlacedShape {
    ColliderShape shape;
    Vec3 position{0.0f};
    Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
};

// Форма тела Jolt из его коллайдеров: одна фигура в начале осей - она сама,
// сдвинутая - RotatedTranslatedShape, несколько - StaticCompoundShape, ни
// одной - EmptyShape (тело без коллайдеров ни с чем не сталкивается). Фигура,
// которую Jolt не принял, пропускается с сообщением в лог.
JPH::RefConst<JPH::Shape> joltBodyShape(std::span<const PlacedShape> shapes);

} // namespace yqlora
