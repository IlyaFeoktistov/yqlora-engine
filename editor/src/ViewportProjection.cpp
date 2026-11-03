#include "yqlora/editor/ViewportProjection.hpp"

namespace yqlora::editor {

namespace {

// Точки ближе к плоскости камеры не проецируются: деление на почти ноль.
constexpr float kMinClipW = 0.01f;

} // namespace

std::optional<Vec2> projectToViewport(const Mat4& toClip, const Vec3& point, const Vec2& viewportSize)
{
    const Vec4 clip = toClip * Vec4(point, 1.0f);
    if (clip.w <= kMinClipW) {
        return std::nullopt;
    }
    const Vec2 ndc = Vec2(clip) / clip.w;

    return Vec2((ndc.x * 0.5f + 0.5f) * viewportSize.x, (0.5f - ndc.y * 0.5f) * viewportSize.y);
}

} // namespace yqlora::editor
