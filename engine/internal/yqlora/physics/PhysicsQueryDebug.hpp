#pragma once

#include "yqlora/debug/DebugDraw.hpp"
#include "yqlora/physics/PhysicsQuery.hpp"

#include <functional>
#include <optional>
#include <span>

namespace yqlora {

class Scene;

// Запросы физики в канале kQueriesDebugChannel: путь луча или фигуры до
// попадания цветом попадания, а промах - на всю длину своим цветом; точка
// и нормаль попадания; пересечение - фигура цветом того, нашла ли она
// что-то. Ничего не делает, если канал не собирается.
class QueryDebug {
public:
    // Фигура запроса с центром в center.
    using Shape = std::function<void(DebugDraw& draw, const Vec3& center, const DebugOptions& options)>;

    explicit QueryDebug(Scene& scene);

    void ray(const Vec3& origin, const Vec3& direction, float maxDistance, std::span<const RaycastHit> hits) const;
    void cast(
        const Shape& shape,
        const Vec3& origin,
        const Vec3& direction,
        float maxDistance,
        const std::optional<RaycastHit>& hit
    ) const;
    void overlap(const Shape& shape, const Vec3& center, bool found) const;

private:
    void drawHit(const RaycastHit& hit) const;

    DebugDraw* m_draw = nullptr;
};

} // namespace yqlora
