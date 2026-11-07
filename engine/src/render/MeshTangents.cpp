#include "yqlora/render/MeshTangents.hpp"

#include <glm/geometric.hpp>
#include <mikktspace.h>

#include <map>
#include <span>

namespace yqlora {

namespace {

// Касательные углов, которые MikkTSpace считает одной вершиной, совпадают до
// округления; дальше этого - разные, и вершина делится.
constexpr float kSameTangent = 1e-4f;

struct Context {
    std::span<const Vertex> vertices;
    std::span<const std::uint32_t> indices;
    // Касательная каждого угла, по порядку indices.
    std::vector<Vec4> corners;
};

Context& contextOf(const SMikkTSpaceContext* context)
{
    return *static_cast<Context*>(context->m_pUserData);
}

const Vertex& cornerVertex(const SMikkTSpaceContext* context, int face, int corner)
{
    const Context& self = contextOf(context);

    return self.vertices
        [self.indices[static_cast<std::size_t>(face) * MeshData::kTriangleCorners + static_cast<std::size_t>(corner)]];
}

int faceCount(const SMikkTSpaceContext* context)
{
    return static_cast<int>(contextOf(context).indices.size() / MeshData::kTriangleCorners);
}

int cornersOfFace(const SMikkTSpaceContext* /*context*/, int /*face*/)
{
    return static_cast<int>(MeshData::kTriangleCorners);
}

void position(const SMikkTSpaceContext* context, float out[], int face, int corner)
{
    const Vec3& value = cornerVertex(context, face, corner).position;
    out[0] = value.x;
    out[1] = value.y;
    out[2] = value.z;
}

void normal(const SMikkTSpaceContext* context, float out[], int face, int corner)
{
    const Vec3& value = cornerVertex(context, face, corner).normal;
    out[0] = value.x;
    out[1] = value.y;
    out[2] = value.z;
}

// MikkTSpace ведёт бинормаль по росту v, а у нас (как в glTF) v растёт вниз
// по картинке, бинормаль же смотрит вверх: v отражается.
void texCoord(const SMikkTSpaceContext* context, float out[], int face, int corner)
{
    const Vec2& value = cornerVertex(context, face, corner).uv0;
    out[0] = value.x;
    out[1] = 1.0f - value.y;
}

void setTangent(const SMikkTSpaceContext* context, const float tangent[], float sign, int face, int corner)
{
    Context& self = contextOf(context);
    self.corners[static_cast<std::size_t>(face) * MeshData::kTriangleCorners + static_cast<std::size_t>(corner)] = Vec4(
        tangent[0],
        tangent[1],
        tangent[2],
        sign
    );
}

bool sameTangent(const Vec4& a, const Vec4& b)
{
    return a.w == b.w && glm::distance(Vec3(a), Vec3(b)) <= kSameTangent;
}

} // namespace

bool computeTangents(std::vector<Vertex>& vertices, std::vector<std::uint32_t>& indices)
{
    const std::size_t triangles = indices.size() / MeshData::kTriangleCorners;
    if (triangles == 0) {
        return false;
    }
    Context self{vertices, indices, std::vector<Vec4>(triangles * MeshData::kTriangleCorners)};
    SMikkTSpaceInterface callbacks{};
    callbacks.m_getNumFaces = &faceCount;
    callbacks.m_getNumVerticesOfFace = &cornersOfFace;
    callbacks.m_getPosition = &position;
    callbacks.m_getNormal = &normal;
    callbacks.m_getTexCoord = &texCoord;
    callbacks.m_setTSpaceBasic = &setTangent;
    SMikkTSpaceContext context{&callbacks, &self};
    if (genTangSpaceDefault(&context) == 0) {
        return false;
    }

    // Вершина берёт касательную первого своего угла; угол с другой получает
    // копию вершины, одну на каждую различную касательную.
    const std::size_t original = vertices.size();
    std::vector<bool> assigned(original, false);
    std::map<std::uint32_t, std::vector<std::uint32_t>> copies;
    for (std::size_t i = 0; i < self.corners.size(); ++i) {
        const std::uint32_t index = indices[i];
        const Vec4& tangent = self.corners[i];
        if (!assigned[index]) {
            vertices[index].tangent = tangent;
            assigned[index] = true;
            continue;
        }
        if (sameTangent(vertices[index].tangent, tangent)) {
            continue;
        }
        std::vector<std::uint32_t>& split = copies[index];
        std::uint32_t target = 0;
        bool found = false;
        for (const std::uint32_t copy : split) {
            if (sameTangent(vertices[copy].tangent, tangent)) {
                target = copy;
                found = true;
                break;
            }
        }
        if (!found) {
            Vertex copy = vertices[index];
            copy.tangent = tangent;
            target = static_cast<std::uint32_t>(vertices.size());
            vertices.push_back(copy);
            split.push_back(target);
        }
        indices[i] = target;
    }

    return true;
}

} // namespace yqlora
