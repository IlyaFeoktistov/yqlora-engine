#include "yqlora/editor/ShapeHandles.hpp"

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtx/quaternion.hpp>

#include <algorithm>
#include <cmath>

namespace yqlora::editor {

namespace {

// Ось Y фигуры у капсулы и цилиндра - своя высота, X и Z - радиус.
constexpr int kAxisY = 1;
// Луч почти параллелен прямой: доля от произведения длин, ниже которой
// точка на прямой не определена.
constexpr float kParallelEpsilon = 1e-6f;

Vec3 axisVector(int axis)
{
    Vec3 vector(0.0f);
    vector[axis] = 1.0f;

    return vector;
}

} // namespace

std::array<ShapeHandle, kShapeHandleCount> shapeHandles()
{
    return {{
        {0, 1.0f},
        {0, -1.0f},
        {1, 1.0f},
        {1, -1.0f},
        {2, 1.0f},
        {2, -1.0f},
    }};
}

Vec3 handlePosition(const HandleShape& shape, const ShapeHandle& handle)
{
    return shape.center + handleDirection(shape, handle) * shape.halfExtents[handle.axis];
}

Vec3 handleDirection(const HandleShape& shape, const ShapeHandle& handle)
{
    return shape.rotation * (axisVector(handle.axis) * handle.side);
}

HandleShape dragShapeHandle(
    const HandleShape& start,
    const ShapeHandle& handle,
    float distance,
    const HandleDragOptions& options
)
{
    const int axis = handle.axis;
    const float size = 2.0f * start.halfExtents[axis];
    float newSize = options.symmetric ? size + 2.0f * distance : size + distance;
    if (options.snapStep > 0.0f) {
        newSize = std::round(newSize / options.snapStep) * options.snapStep;
    }
    newSize = std::max(newSize, 2.0f * options.minHalfExtent);
    const float half = newSize * 0.5f;

    HandleShape result = start;
    if (!options.symmetric) {
        // Противоположная сторона стоит: центр уходит на половину прироста.
        result.center = start.center + handleDirection(start, handle) * ((newSize - size) * 0.5f);
    }
    switch (start.links) {
    case ShapeLinks::None: result.halfExtents[axis] = half; break;
    case ShapeLinks::All: result.halfExtents = Vec3(half); break;
    case ShapeLinks::Radial:
        if (axis == kAxisY) {
            result.halfExtents[axis] = half;
        } else {
            result.halfExtents.x = half;
            result.halfExtents.z = half;
        }
        break;
    }

    return result;
}

std::optional<float> closestOnLine(const Ray& ray, const Vec3& origin, const Vec3& direction)
{
    // Ближайшие точки двух прямых: origin + t * direction и ray.
    const Vec3 between = origin - ray.origin;
    const float alongBoth = glm::dot(direction, ray.direction);
    const float rayLength2 = glm::dot(ray.direction, ray.direction);
    const float denominator = rayLength2 - alongBoth * alongBoth;
    if (denominator <= kParallelEpsilon * rayLength2) {
        return std::nullopt;
    }

    return (alongBoth * glm::dot(ray.direction, between) - rayLength2 * glm::dot(direction, between)) / denominator;
}

} // namespace yqlora::editor
