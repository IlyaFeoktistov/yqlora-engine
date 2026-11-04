#include "yqlora/render/ShadowCascades.hpp"

#include "yqlora/render/Prepare.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace yqlora {

namespace {

// Доля логарифмической схемы в границах каскадов.
constexpr float kLogarithmicShare = 0.75f;
// Радиус шара каскада округляется вверх до этого шага: иначе ошибка
// округления меняла бы размер текселя от кадра к кадру.
constexpr float kRadiusStep = 1.0f / 16.0f;
constexpr float kVerticalCosine = 0.99f;
constexpr int kCorners = 4;

struct CameraCorners {
    // Углы ближней и дальней плоскостей в мире.
    std::array<Vec3, kCorners> nearCorners{};
    std::array<Vec3, kCorners> farCorners{};
    // Их глубина вдоль взгляда камеры.
    float nearDepth = 0.0f;
    float farDepth = 0.0f;
};

CameraCorners cameraCorners(const CameraData& camera)
{
    const Mat4 inverse = glm::inverse(camera.projection * camera.view);
    const Vec3 forward = -Vec3(camera.view[0][2], camera.view[1][2], camera.view[2][2]);
    CameraCorners corners;
    const std::array<Vec2, kCorners> clip{{{-1.0f, -1.0f}, {1.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 1.0f}}};
    for (std::size_t i = 0; i < clip.size(); ++i) {
        const Vec4 nearPoint = inverse * Vec4(clip[i], 0.0f, 1.0f);
        const Vec4 farPoint = inverse * Vec4(clip[i], 1.0f, 1.0f);
        corners.nearCorners[i] = Vec3(nearPoint) / nearPoint.w;
        corners.farCorners[i] = Vec3(farPoint) / farPoint.w;
    }
    corners.nearDepth = glm::dot(corners.nearCorners[0] - camera.position, forward);
    corners.farDepth = glm::dot(corners.farCorners[0] - camera.position, forward);

    return corners;
}

// Точка ребра пирамиды i на глубине depth: глубина вдоль ребра линейна.
Vec3 cornerAt(const CameraCorners& corners, std::size_t i, float depth)
{
    const float t = (depth - corners.nearDepth) / (corners.farDepth - corners.nearDepth);

    return glm::mix(corners.nearCorners[i], corners.farCorners[i], t);
}

} // namespace

std::vector<float> cascadeSplits(float near, float distance, int count)
{
    std::vector<float> splits;
    const float far = std::max(distance, near * 2.0f);
    for (int i = 1; i <= count; ++i) {
        const float share = static_cast<float>(i) / static_cast<float>(count);
        const float logarithmic = near * std::pow(far / near, share);
        const float uniform = near + (far - near) * share;
        splits.push_back(glm::mix(uniform, logarithmic, kLogarithmicShare));
    }
    splits.back() = far;

    return splits;
}

std::vector<ShadowCascade> fitShadowCascades(
    const CameraData& camera,
    const Vec3& toLight,
    const ShadowSettings& settings,
    std::span<const DrawItem> items
)
{
    const CameraCorners corners = cameraCorners(camera);
    const float distance = std::min(settings.distance, corners.farDepth);
    const std::vector<float> splits = cascadeSplits(corners.nearDepth, distance, settings.cascades);
    // Пространство света: взгляд вдоль света (-toLight), к солнцу - +Z.
    const Vec3 up = std::abs(toLight.y) > kVerticalCosine ? Vec3(1.0f, 0.0f, 0.0f) : Vec3(0.0f, 1.0f, 0.0f);
    const Mat4 lightView = glm::lookAt(Vec3(0.0f), -toLight, up);

    // Отбрасывающие тень в пространстве света, один раз на все каскады.
    std::vector<Bounds> casters;
    for (const DrawItem& item : items) {
        if (item.castShadows) {
            casters.push_back(transformBounds(item.bounds, lightView));
        }
    }

    std::vector<ShadowCascade> cascades;
    float splitNear = corners.nearDepth;
    for (const float splitFar : splits) {
        std::array<Vec3, kCorners * 2> slice{};
        Vec3 center(0.0f);
        for (std::size_t i = 0; i < kCorners; ++i) {
            slice[i] = cornerAt(corners, i, splitNear);
            slice[i + kCorners] = cornerAt(corners, i, splitFar);
        }
        for (const Vec3& point : slice) {
            center += point / static_cast<float>(slice.size());
        }
        float radius = 0.0f;
        for (const Vec3& point : slice) {
            radius = std::max(radius, glm::length(point - center));
        }
        radius = std::ceil(radius / kRadiusStep) * kRadiusStep;

        // Центр в пространстве света, привязанный к текселям карты.
        const float texel = 2.0f * radius / static_cast<float>(settings.cascadeSize);
        Vec3 lightCenter = Vec3(lightView * Vec4(center, 1.0f));
        lightCenter.x = std::floor(lightCenter.x / texel) * texel;
        lightCenter.y = std::floor(lightCenter.y / texel) * texel;

        float top = lightCenter.z + radius;
        const float bottom = lightCenter.z - radius;
        for (const Bounds& caster : casters) {
            const bool overlaps = caster.max.x >= lightCenter.x - radius && caster.min.x <= lightCenter.x + radius
                                  && caster.max.y >= lightCenter.y - radius && caster.min.y <= lightCenter.y + radius
                                  && caster.max.z >= bottom;
            if (overlaps) {
                top = std::max(top, caster.max.z);
            }
        }
        // orthoRH_ZO берёт ближнюю и дальнюю как расстояния вдоль -Z.
        const Mat4 projection = glm::orthoRH_ZO(
            lightCenter.x - radius,
            lightCenter.x + radius,
            lightCenter.y - radius,
            lightCenter.y + radius,
            -top,
            -bottom
        );
        cascades.push_back({
            .viewProjection = projection * lightView,
            .splitFar = splitFar,
            .width = 2.0f * radius,
            .depthRange = top - bottom,
        });
        splitNear = splitFar;
    }

    return cascades;
}

} // namespace yqlora
