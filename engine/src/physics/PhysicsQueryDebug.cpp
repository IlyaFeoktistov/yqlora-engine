#include "yqlora/physics/PhysicsQueryDebug.hpp"

#include "yqlora/physics/PhysicsDebug.hpp"
#include "yqlora/scene/Scene.hpp"

namespace yqlora {

namespace {

const Vec4 kHitColor(0.3f, 1.0f, 0.3f, 1.0f);
const Vec4 kMissColor(1.0f, 0.35f, 0.35f, 1.0f);
const Vec4 kNormalColor(0.3f, 0.6f, 1.0f, 1.0f);

// Крест точки попадания и стрелка нормали, м.
constexpr float kHitPointSize = 0.1f;
constexpr float kHitNormalLength = 0.4f;

DebugOptions queryOptions(const Vec4& color)
{
    return {.color = color, .channel = kQueriesDebugChannel};
}

// Единичное направление; нулевое - без пути.
Vec3 unit(const Vec3& direction)
{
    const float length = glm::length(direction);

    return length > math::kEpsilon ? direction / length : Vec3(0.0f);
}

} // namespace

QueryDebug::QueryDebug(Scene& scene)
{
    DebugDraw* draw = scene.system<DebugDraw>();
    if (draw && draw->collecting(kQueriesDebugChannel)) {
        m_draw = draw;
    }
}

void QueryDebug::drawHit(const RaycastHit& hit) const
{
    m_draw->point(hit.point, kHitPointSize, queryOptions(kHitColor));
    m_draw->arrow(hit.point, hit.point + hit.normal * kHitNormalLength, queryOptions(kNormalColor));
}

void QueryDebug::ray(
    const Vec3& origin,
    const Vec3& direction,
    float maxDistance,
    std::span<const RaycastHit> hits
) const
{
    if (!m_draw) {
        return;
    }
    const Vec3 path = unit(direction) * maxDistance;
    if (hits.empty()) {
        m_draw->ray(origin, path, queryOptions(kMissColor));

        return;
    }
    // Попадания - по расстоянию: путь до дальнего, дальше - остаток промаха.
    const Vec3 last = origin + unit(direction) * hits.back().distance;
    m_draw->line(origin, last, queryOptions(kHitColor));
    m_draw->line(last, origin + path, queryOptions(kMissColor));
    for (const RaycastHit& hit : hits) {
        drawHit(hit);
    }
}

void QueryDebug::cast(
    const Shape& shape,
    const Vec3& origin,
    const Vec3& direction,
    float maxDistance,
    const std::optional<RaycastHit>& hit
) const
{
    if (!m_draw) {
        return;
    }
    const Vec4 color = hit ? kHitColor : kMissColor;
    const Vec3 end = origin + unit(direction) * (hit ? hit->distance : maxDistance);
    shape(*m_draw, origin, queryOptions(color));
    m_draw->line(origin, end, queryOptions(color));
    shape(*m_draw, end, queryOptions(color));
    if (hit) {
        drawHit(*hit);
    }
}

void QueryDebug::overlap(const Shape& shape, const Vec3& center, bool found) const
{
    if (m_draw) {
        shape(*m_draw, center, queryOptions(found ? kHitColor : kMissColor));
    }
}

} // namespace yqlora
