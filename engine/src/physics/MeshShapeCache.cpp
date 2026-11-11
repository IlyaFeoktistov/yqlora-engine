// Jolt.h - первым: остальные заголовки Jolt на него опираются.
#include <Jolt/Jolt.h>

#include "yqlora/physics/MeshShapeCache.hpp"

#include "yqlora/core/Log.hpp"
#include "yqlora/physics/JoltMath.hpp"
#include "yqlora/render/MeshData.hpp"
#include "yqlora/resource/ResourceCache.hpp"

#include <Jolt/Geometry/IndexedTriangle.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>

namespace yqlora {

namespace {

constexpr std::size_t kTriangleCorners = 3;

JPH::RefConst<JPH::Shape> cook(const MeshData& mesh, bool convex, ResourceId id)
{
    JPH::ShapeSettings::ShapeResult result;
    if (convex) {
        JPH::Array<JPH::Vec3> points;
        points.reserve(mesh.vertices.size());
        for (const Vertex& vertex : mesh.vertices) {
            points.push_back(toJolt(vertex.position));
        }
        result = JPH::ConvexHullShapeSettings(points).Create();
    } else {
        JPH::VertexList vertices;
        vertices.reserve(mesh.vertices.size());
        for (const Vertex& vertex : mesh.vertices) {
            vertices.push_back(JPH::Float3(vertex.position.x, vertex.position.y, vertex.position.z));
        }
        JPH::IndexedTriangleList triangles;
        triangles.reserve(mesh.indices.size() / kTriangleCorners);
        for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += kTriangleCorners) {
            triangles.push_back(JPH::IndexedTriangle(mesh.indices[i], mesh.indices[i + 1], mesh.indices[i + 2]));
        }
        result = JPH::MeshShapeSettings(std::move(vertices), std::move(triangles)).Create();
    }
    if (result.HasError()) {
        log::error("Physics: mesh {:016x} gives no shape: {}", id, result.GetError().c_str());

        return nullptr;
    }

    return result.Get();
}

} // namespace

std::uint64_t MeshShapeCache::revisionOf(ResourceCache& resources, ResourceId mesh)
{
    return resources.get<MeshData>(mesh).revision;
}

JPH::RefConst<JPH::Shape> MeshShapeCache::shape(ResourceCache& resources, ResourceId mesh, bool convex)
{
    const LoadedData<MeshData> data = resources.get<MeshData>(mesh);
    if (!data || data.data->vertices.empty()) {
        return nullptr;
    }
    Entry& entry = m_entries[{mesh, convex}];
    // Неудача тоже запоминается: тот же меш не готовится и не ругается снова.
    if (entry.revision != data.revision) {
        entry = {data.revision, cook(*data.data, convex, mesh)};
        ++m_cooked;
    }

    return entry.shape;
}

} // namespace yqlora
