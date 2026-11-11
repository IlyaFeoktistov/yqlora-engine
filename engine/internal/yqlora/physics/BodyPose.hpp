#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/math/Math.hpp"

namespace yqlora {

// Положение и поворот тела в мире; масштаб узла к телу не относится.
struct BodyPose {
    Vec3 position{0.0f};
    Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};

    bool operator==(const BodyPose&) const = default;
};

// Положение и поворот из мировой матрицы узла: масштаб и наклон отброшены.
// Одна и та же матрица всегда даёт одно и то же, поэтому сравнение с
// запомненным точное.
YQLORA_API BodyPose bodyPoseOf(const Mat4& world);

// Мировая матрица с положением и поворотом pose и масштабом осей из world.
YQLORA_API Mat4 worldWithPose(const Mat4& world, const BodyPose& pose);

// Между a и b на долю t (0 - a, 1 - b): положение по прямой, поворот по дуге.
YQLORA_API BodyPose blendPoses(const BodyPose& a, const BodyPose& b, float t);

} // namespace yqlora
