#pragma once

#include "yqlora/math/Math.hpp"

#include <optional>
#include <vector>

namespace yqlora {
class Joint;
class Node3D;
class Scene;
} // namespace yqlora

namespace yqlora::editor {

// Рамка скрепления в мире так, как её строит физика: точка крепления, ось
// и нуль угла (нормаль), без масштаба узла.
struct JointWorldFrame {
    Vec3 point{0.0f};
    Vec3 axis{0.0f, 1.0f, 0.0f};
    Vec3 normal{1.0f, 0.0f, 0.0f};
};

JointWorldFrame jointWorldFrame(const Joint& joint, const Node3D& node);

// Точки дуги радиуса radius вокруг оси рамки от угла fromDegrees до
// toDegrees: нуль - нормаль рамки, положительный угол - против часовой
// стрелки, если смотреть с конца оси (как угол петли). segments отрезков.
std::vector<Vec3> jointArc(
    const JointWorldFrame& frame,
    float fromDegrees,
    float toDegrees,
    float radius,
    int segments
);

// Точка крепления на другом теле в мире: у DistanceJoint - его
// connectedAnchor, у остальных - положение узла другого тела. nullopt -
// скрепление с миром без своей точки или узел не найден.
std::optional<Vec3> connectedPoint(const Joint& joint, const Scene& scene);

} // namespace yqlora::editor
