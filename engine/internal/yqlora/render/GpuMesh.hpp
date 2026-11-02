#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/render/GpuResourceCache.hpp"
#include "yqlora/rhi/Device.hpp"

#include <cstdint>
#include <memory>

namespace yqlora {

struct DrawItem;
struct MeshData;

// Входы вершин меша. Те же location у шейдеров мешей.
inline constexpr std::uint32_t kPositionLocation = 0;
inline constexpr std::uint32_t kNormalLocation = 1;
inline constexpr std::uint32_t kUv0Location = 2;

// Раскладка Vertex для конвейеров, которые рисуют меши.
YQLORA_API rhi::VertexLayout meshVertexLayout();

// Меш в видеопамяти.
struct GpuMesh {
    std::unique_ptr<rhi::Buffer> vertices;
    std::unique_ptr<rhi::Buffer> indices;
    std::uint32_t indexCount = 0;
};

YQLORA_API std::unique_ptr<GpuMesh> uploadMesh(rhi::Device& device, const MeshData& data);

// Буферы меша и все его треугольники; конвейер уже выбран.
YQLORA_API void drawMesh(rhi::CommandList& commands, const GpuMesh& mesh);

// Копии мешей кадра в видеопамяти, см. GpuResourceCache.
class YQLORA_API GpuMeshCache {
public:
    explicit GpuMeshCache(rhi::Device& device)
        : m_device(device)
    {
    }

    const GpuMesh& get(const DrawItem& item);

    void endFrame()
    {
        m_meshes.endFrame();
    }

private:
    rhi::Device& m_device;
    GpuResourceCache<GpuMesh> m_meshes;
};

} // namespace yqlora
