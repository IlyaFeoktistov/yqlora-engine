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

constexpr std::uint32_t kRoundSegments = 32; // по кругу у сферы, капсулы, цилиндра
constexpr float kRoundRadius = 0.5f;
constexpr float kTallHeight = 2.0f;

// Касательная вдоль долготы: u растёт с углом phi.
Vec4 roundTangent(float phi)
{
    return {std::cos(phi), 0.0f, -std::sin(phi), 1.0f};
}

float segmentAngle(std::uint32_t segment)
{
    return glm::two_pi<float>() * static_cast<float>(segment) / kRoundSegments;
}

// Полосы между соседними рядами по kRoundSegments + 1 вершин (шов по
// долготе), с того ряда, что начинается с вершины first.
void addBands(MeshData& mesh, std::uint32_t first, std::uint32_t rows)
{
    constexpr std::uint32_t kRowSize = kRoundSegments + 1;
    for (std::uint32_t row = 0; row + 1 < rows; ++row) {
        for (std::uint32_t segment = 0; segment < kRoundSegments; ++segment) {
            const std::uint32_t top = first + row * kRowSize + segment;
            const std::uint32_t bottom = top + kRowSize;
            mesh.indices.insert(mesh.indices.end(), {top, bottom, bottom + 1, top, bottom + 1, top + 1});
        }
    }
}

// Круглая крышка цилиндра на высоте y; нормаль вверх (up) или вниз. uv -
// проекция сверху, u вдоль X, как у грани куба.
void addCap(MeshData& mesh, float y, bool up)
{
    const Vec3 normal(0.0f, up ? 1.0f : -1.0f, 0.0f);
    const Vec4 tangent(1.0f, 0.0f, 0.0f, 1.0f);
    const float vSign = up ? 1.0f : -1.0f;
    const auto center = static_cast<std::uint32_t>(mesh.vertices.size());
    mesh.vertices.push_back({{0.0f, y, 0.0f}, normal, {0.5f, 0.5f}, tangent});
    for (std::uint32_t segment = 0; segment <= kRoundSegments; ++segment) {
        const float phi = segmentAngle(segment);
        const float x = std::sin(phi) * kRoundRadius;
        const float z = std::cos(phi) * kRoundRadius;
        mesh.vertices.push_back({{x, y, z}, normal, {0.5f + x, 0.5f + vSign * z}, tangent});
    }
    for (std::uint32_t segment = 0; segment < kRoundSegments; ++segment) {
        const std::uint32_t ring = center + 1 + segment;
        if (up) {
            mesh.indices.insert(mesh.indices.end(), {center, ring, ring + 1});
        } else {
            mesh.indices.insert(mesh.indices.end(), {ring, center, ring + 1});
        }
    }
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
    constexpr std::uint32_t kRings = 16; // по широте

    MeshData mesh;
    for (std::uint32_t ring = 0; ring <= kRings; ++ring) {
        const float theta = glm::pi<float>() * static_cast<float>(ring) / kRings;
        for (std::uint32_t segment = 0; segment <= kRoundSegments; ++segment) {
            const float phi = segmentAngle(segment);
            const Vec3 normal(std::sin(theta) * std::sin(phi), std::cos(theta), std::sin(theta) * std::cos(phi));
            // Развёртка по долготе и широте, v = 0 у верхнего полюса.
            const Vec2 uv(static_cast<float>(segment) / kRoundSegments, static_cast<float>(ring) / kRings);
            mesh.vertices.push_back({normal * kRoundRadius, normal, uv, roundTangent(phi)});
        }
    }
    addBands(mesh, 0, kRings + 1);
    finishSingleMaterial(mesh);

    return mesh;
}

MeshData makeCapsule()
{
    // Полусфера сверху, затем снизу; ряды экватора повторены сдвинутыми на
    // половину прямой части - между ними цилиндрическая полоса.
    constexpr std::uint32_t kHalfRings = 8;
    constexpr float kHalfStraight = kTallHeight * 0.5f - kRoundRadius;
    // v растёт вниз по длине поверхности от верхнего полюса до нижнего.
    constexpr float kArc = glm::half_pi<float>() * kRoundRadius;
    constexpr float kLength = 2.0f * kArc + 2.0f * kHalfStraight;

    MeshData mesh;
    for (std::uint32_t half = 0; half < 2; ++half) {
        const float offset = half == 0 ? kHalfStraight : -kHalfStraight;
        for (std::uint32_t ring = 0; ring <= kHalfRings; ++ring) {
            const float t = static_cast<float>(ring) / kHalfRings;
            const float theta = glm::half_pi<float>() * (static_cast<float>(half) + t);
            const float distance = half == 0 ? t * kArc : kArc + 2.0f * kHalfStraight + t * kArc;
            for (std::uint32_t segment = 0; segment <= kRoundSegments; ++segment) {
                const float phi = segmentAngle(segment);
                const Vec3 normal(std::sin(theta) * std::sin(phi), std::cos(theta), std::sin(theta) * std::cos(phi));
                const Vec2 uv(static_cast<float>(segment) / kRoundSegments, distance / kLength);
                mesh.vertices.push_back(
                    {normal * kRoundRadius + Vec3(0.0f, offset, 0.0f), normal, uv, roundTangent(phi)}
                );
            }
        }
    }
    addBands(mesh, 0, 2 * (kHalfRings + 1));
    finishSingleMaterial(mesh);

    return mesh;
}

MeshData makeCylinder()
{
    constexpr float kHalfHeight = kTallHeight * 0.5f;

    MeshData mesh;
    // Бок: два ряда со своими нормалями, отдельно от крышек - ребро острое.
    for (const float y : {kHalfHeight, -kHalfHeight}) {
        const float v = y > 0.0f ? 0.0f : 1.0f;
        for (std::uint32_t segment = 0; segment <= kRoundSegments; ++segment) {
            const float phi = segmentAngle(segment);
            const Vec3 normal(std::sin(phi), 0.0f, std::cos(phi));
            const Vec2 uv(static_cast<float>(segment) / kRoundSegments, v);
            mesh.vertices.push_back({normal * kRoundRadius + Vec3(0.0f, y, 0.0f), normal, uv, roundTangent(phi)});
        }
    }
    addBands(mesh, 0, 2);
    addCap(mesh, kHalfHeight, true);
    addCap(mesh, -kHalfHeight, false);
    finishSingleMaterial(mesh);

    return mesh;
}

} // namespace yqlora
