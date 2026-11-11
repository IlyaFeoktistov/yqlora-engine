#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/math/Math.hpp"
#include "yqlora/resource/ResourceId.hpp"

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

// Треугольники меша (convex false) или их выпуклая оболочка (true).
struct MeshShape {
    ResourceId mesh = 0;
    bool convex = false;
};

using ColliderShape = std::variant<BoxShape, SphereShape, CapsuleShape, CylinderShape, MeshShape>;

// Фигура под масштабом осей scale (масштаб узлов до тела): размеры
// умножаются на него. Шар берёт наибольшую ось, капсула и цилиндр -
// наибольшую из X и Z для радиуса, высота - по Y (полусферы капсулы
// остаются круглыми). uniformLost - оси, которые фигура должна была бы
// держать равными, разные: шар или круг сечения уже не тот, что на экране.
// Меш не меняется: его масштаб - у формы физики.
YQLORA_API ColliderShape scaledShape(const ColliderShape& shape, const Vec3& scale, bool& uniformLost);

} // namespace yqlora
