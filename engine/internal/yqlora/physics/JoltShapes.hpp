#pragma once

// Только для кода движка, который включает Jolt (Jolt.h - раньше этого файла).

#include "yqlora/math/Math.hpp"
#include "yqlora/physics/ColliderShape.hpp"

#include <Jolt/Physics/Collision/PhysicsMaterial.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>

#include <cstdint>
#include <functional>
#include <span>

namespace yqlora {

// Фигура коллайдера на своём месте в осях тела. Масштаб простых фигур уже
// в их размерах (scaledShape); scale - для меша. material - физический
// материал коллайдера (PhysicsMaterialCache); part - номер коллайдера в
// теле, по нему контакт находит коллайдер (bodyPartOf).
struct PlacedShape {
    ColliderShape shape;
    std::uint32_t part = 0;
    const JPH::PhysicsMaterial* material = nullptr;
    Vec3 position{0.0f};
    Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    Vec3 scale{1.0f};
};

// Готовая форма меша с материалом (MeshShapeCache); nullptr - формы нет.
using MeshShapeSource = std::function<
    JPH::RefConst<JPH::Shape>(ResourceId mesh, bool convex, const JPH::PhysicsMaterial* material)>;

// Форма тела и номер коллайдера, если фигура в ней одна.
struct BodyShape {
    JPH::RefConst<JPH::Shape> shape;
    std::uint32_t solePart = 0;
};

// Форма тела Jolt из его коллайдеров: одна фигура в начале осей - она сама,
// сдвинутая - RotatedTranslatedShape, несколько - StaticCompoundShape с
// part в данных каждой части, ни одной - EmptyShape (тело без коллайдеров
// ни с чем не сталкивается). Фигура, которую Jolt не принял, пропускается с
// сообщением в лог.
BodyShape joltBodyShape(std::span<const PlacedShape> shapes, const MeshShapeSource& meshes);

// Части составной формы нет - номер коллайдера один на тело (BodyShape::solePart).
inline constexpr std::uint32_t kSolePart = ~std::uint32_t{0};

// Номер коллайдера подфигуры part формы тела shape; kSolePart - форма не
// составная. Можно из рабочих потоков: форма только читается.
std::uint32_t bodyPartOf(const JPH::Shape& shape, const JPH::SubShapeID& part);

} // namespace yqlora
