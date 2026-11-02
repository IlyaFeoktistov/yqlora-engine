#include "yqlora/render/GpuMesh.hpp"

#include "yqlora/render/FrameData.hpp"
#include "yqlora/render/MeshData.hpp"

#include <cstddef>
#include <span>

namespace yqlora {

rhi::VertexLayout meshVertexLayout()
{
    return {
        .stride = sizeof(Vertex),
        .attributes = {
            {kPositionLocation, rhi::VertexFormat::Float3, offsetof(Vertex, position)},
            {kNormalLocation, rhi::VertexFormat::Float3, offsetof(Vertex, normal)},
            {kUv0Location, rhi::VertexFormat::Float2, offsetof(Vertex, uv0)},
        },
    };
}

std::unique_ptr<GpuMesh> uploadMesh(rhi::Device& device, const MeshData& data)
{
    const std::span<const std::byte> vertices = std::as_bytes(std::span(data.vertices));
    const std::span<const std::byte> indices = std::as_bytes(std::span(data.indices));

    auto mesh = std::make_unique<GpuMesh>();
    mesh->vertices = device.createBuffer({.size = vertices.size(), .usage = rhi::BufferUsage::Vertex}, vertices);
    mesh->indices = device.createBuffer({.size = indices.size(), .usage = rhi::BufferUsage::Index}, indices);
    mesh->indexCount = static_cast<std::uint32_t>(data.indices.size());

    return mesh;
}

void drawMesh(rhi::CommandList& commands, const GpuMesh& mesh)
{
    if (mesh.indexCount == 0) {
        return;
    }
    commands.setVertexBuffer(*mesh.vertices);
    commands.setIndexBuffer(*mesh.indices);
    commands.drawIndexed(mesh.indexCount);
}

const GpuMesh& GpuMeshCache::get(const DrawItem& item)
{
    // Старую копию можно отдать сразу, даже если ею уже рисовали в этом
    // кадре: освобождение ждёт конца кадров (см. rhi::Device).
    return m_meshes.get(item.meshId, item.meshRevision, [&] {
        return uploadMesh(m_device, *item.mesh);
    });
}

} // namespace yqlora
