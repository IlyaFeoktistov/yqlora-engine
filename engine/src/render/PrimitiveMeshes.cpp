#include "yqlora/render/PrimitiveMeshes.hpp"

#include <glm/gtc/constants.hpp>

#include <cmath>

namespace yqlora {

namespace {

// Квадрат со стороной 1 и центром в center. right и up задают его плоскость,
// нормаль смотрит в сторону cross(right, up).
void addQuad(MeshData& mesh, const Vec3& center, const Vec3& right, const Vec3& up)
{
    const Vec3 normal = glm::normalize(glm::cross(right, up));
    const auto first = static_cast<std::uint32_t>(mesh.vertices.size());
    const Vec3 halfRight = right * 0.5f;
    const Vec3 halfUp = up * 0.5f;

    // uv0 как в glTF: (0, 0) - левый верхний угол картинки, v растёт вниз.
    // u растёт вдоль right, верх картинки - вдоль up = cross(normal, right).
    const Vec4 tangent(glm::normalize(right), 1.0f);
    mesh.vertices.push_back({center - halfRight - halfUp, normal, {0.0f, 1.0f}, tangent});
    mesh.vertices.push_back({center + halfRight - halfUp, normal, {1.0f, 1.0f}, tangent});
    mesh.vertices.push_back({center + halfRight + halfUp, normal, {1.0f, 0.0f}, tangent});
    mesh.vertices.push_back({center - halfRight + halfUp, normal, {0.0f, 0.0f}, tangent});
    mesh.indices.insert(mesh.indices.end(), {first, first + 1, first + 2, first, first + 2, first + 3});
}

} // namespace

MeshData makeCube()
{
    // Для каждой грани свои вершины, чтобы нормали были плоскими и рёбра острыми.
    MeshData mesh;
    const Vec3 x(1.0f, 0.0f, 0.0f);
    const Vec3 y(0.0f, 1.0f, 0.0f);
    const Vec3 z(0.0f, 0.0f, 1.0f);
    addQuad(mesh, z * 0.5f, x, y);
    addQuad(mesh, -z * 0.5f, -x, y);
    addQuad(mesh, x * 0.5f, -z, y);
    addQuad(mesh, -x * 0.5f, z, y);
    addQuad(mesh, y * 0.5f, x, -z);
    addQuad(mesh, -y * 0.5f, x, z);
    finishSingleMaterial(mesh);

    return mesh;
}

MeshData makePlane()
{
    MeshData mesh;
    addQuad(mesh, Vec3(0.0f), Vec3(1.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f));
    finishSingleMaterial(mesh);

    return mesh;
}

MeshData makeSphere()
{
    constexpr std::uint32_t kSegments = 32; // по долготе
    constexpr std::uint32_t kRings = 16;    // по широте
    constexpr float kRadius = 0.5f;

    MeshData mesh;
    for (std::uint32_t ring = 0; ring <= kRings; ++ring) {
        const float theta = glm::pi<float>() * static_cast<float>(ring) / kRings;
        for (std::uint32_t segment = 0; segment <= kSegments; ++segment) {
            const float phi = glm::two_pi<float>() * static_cast<float>(segment) / kSegments;
            const Vec3 normal(std::sin(theta) * std::sin(phi), std::cos(theta), std::sin(theta) * std::cos(phi));
            // Развёртка по долготе и широте, v = 0 у верхнего полюса.
            const Vec2 uv(static_cast<float>(segment) / kSegments, static_cast<float>(ring) / kRings);
            // u растёт с долготой: касательная - производная положения по ней.
            const Vec4 tangent(std::cos(phi), 0.0f, -std::sin(phi), 1.0f);
            mesh.vertices.push_back({normal * kRadius, normal, uv, tangent});
        }
    }

    // Шов по долготе: у первой и последней вершины ряда одинаковое положение.
    constexpr std::uint32_t kRowSize = kSegments + 1;
    for (std::uint32_t ring = 0; ring < kRings; ++ring) {
        for (std::uint32_t segment = 0; segment < kSegments; ++segment) {
            const std::uint32_t top = ring * kRowSize + segment;
            const std::uint32_t bottom = top + kRowSize;
            mesh.indices.insert(mesh.indices.end(), {top, bottom, bottom + 1, top, bottom + 1, top + 1});
        }
    }
    finishSingleMaterial(mesh);

    return mesh;
}

} // namespace yqlora
