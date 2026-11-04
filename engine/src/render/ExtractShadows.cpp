#include "yqlora/render/FrameExtract.hpp"

#include "yqlora/core/Log.hpp"
#include "yqlora/render/GraphicsSettings.hpp"
#include "yqlora/render/RenderSystem.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>

namespace yqlora {

namespace {

// Грани куба точечного: куда смотрит грань и её верх, в порядке +X, -X,
// +Y, -Y, +Z, -Z. Шейдер выбирает грань по наибольшей оси в том же порядке.
struct CubeFace {
    Vec3 forward;
    Vec3 up;
};

const std::array<CubeFace, kPointShadowFaces> kCubeFaces{{
    {{1.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}},
    {{-1.0f, 0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}},
    {{0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
    {{0.0f, -1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
    {{0.0f, 0.0f, 1.0f}, {0.0f, -1.0f, 0.0f}},
    {{0.0f, 0.0f, -1.0f}, {0.0f, -1.0f, 0.0f}},
}};

constexpr float kCubeFaceFovDegrees = 90.0f;
constexpr float kMinSpotHalfFovDegrees = 0.5f;
// Почти вертикальный прожектор берёт другой верх, иначе lookAt вырождается.
constexpr float kVerticalCosine = 0.99f;

int tilesOf(const LightData& light)
{
    return light.kind == LightKind::Point ? kPointShadowFaces : 1;
}

} // namespace

ShadowSettings shadowSettings(const GraphicsSettings& settings)
{
    constexpr float kMinDistance = 1.0f;
    constexpr int kMinMapSize = 256;
    constexpr int kMaxMapSize = 8192;

    return {
        .enabled = settings.shadows,
        .distance = std::max(settings.shadowDistance, kMinDistance),
        .cascades = std::clamp(settings.shadowCascades, 1, kMaxShadowCascades),
        .cascadeSize = std::clamp(settings.shadowCascadeSize, kMinMapSize, kMaxMapSize),
        .atlasSize = std::clamp(settings.shadowAtlasSize, kMinMapSize, kMaxMapSize),
        .blockerSamples = std::clamp(settings.shadowBlockerSamples, 1, kMaxShadowSamples),
        .filterSamples = std::clamp(settings.shadowFilterSamples, 1, kMaxShadowSamples),
        .maxPenumbra = std::clamp(settings.shadowMaxPenumbra, kMinShadowPenumbra, kMaxShadowPenumbra),
    };
}

float shadowTileHalfFov(const LightData& light)
{
    if (light.kind == LightKind::Point) {
        return glm::radians(kCubeFaceFovDegrees) * 0.5f;
    }
    // Конус прожектора целиком; узкий - не уже kMinSpotHalfFovDegrees.
    const float half = std::acos(std::clamp(light.cosOuter, 0.0f, 1.0f));

    return std::max(half, glm::radians(kMinSpotHalfFovDegrees));
}

float shadowTileFar(const LightData& light)
{
    return std::max(light.range, kShadowNearPlane * 2.0f);
}

Mat4 shadowTileMatrix(const LightData& light, int face)
{
    const Mat4
        projection = math::perspective(2.0f * shadowTileHalfFov(light), 1.0f, kShadowNearPlane, shadowTileFar(light));
    if (light.kind == LightKind::Point) {
        const CubeFace& cube = kCubeFaces[static_cast<std::size_t>(face)];

        return projection * glm::lookAt(light.position, light.position + cube.forward, cube.up);
    }
    const Vec3 up = std::abs(light.direction.y) > kVerticalCosine ? Vec3(1.0f, 0.0f, 0.0f) : Vec3(0.0f, 1.0f, 0.0f);

    return projection * glm::lookAt(light.position, light.position + light.direction, up);
}

void extractShadowTiles(const RenderSystem& render, const Vec3& camera, FrameData& frame)
{
    if (!frame.shadows.enabled) {
        for (LightData& light : frame.lights) {
            light.shadows = ShadowMode::None;
        }

        return;
    }
    std::vector<std::size_t> wanting;
    for (std::size_t i = 0; i < frame.lights.size(); ++i) {
        const LightData& light = frame.lights[i];
        if (light.kind != LightKind::Directional && light.shadows != ShadowMode::None) {
            wanting.push_back(i);
        }
    }
    // stable: при равном расстоянии порядок сцены, тени не мигают.
    std::ranges::stable_sort(wanting, {}, [&](std::size_t index) {
        const Vec3 offset = frame.lights[index].position - camera;

        return glm::dot(offset, offset);
    });
    bool dropped = false;
    for (const std::size_t index : wanting) {
        LightData& light = frame.lights[index];
        const int count = tilesOf(light);
        if (static_cast<int>(frame.shadowTiles.size()) + count > kMaxShadowTiles) {
            light.shadows = ShadowMode::None;
            dropped = true;
            continue;
        }
        light.firstShadowTile = static_cast<int>(frame.shadowTiles.size());
        for (int face = 0; face < count; ++face) {
            frame.shadowTiles.push_back({.viewProjection = shadowTileMatrix(light, face), .light = index});
        }
    }
    if (dropped && render.takeReport(RenderReport::ShadowTileLimit)) {
        log::warn(
            "Lights with shadows need more than {} shadow atlas tiles (a point light takes {}): the nearest to "
            "the camera keep their shadows",
            kMaxShadowTiles,
            kPointShadowFaces
        );
    }
}

} // namespace yqlora
