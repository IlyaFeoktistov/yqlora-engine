#include "yqlora/render/GpuMaterial.hpp"

#include "yqlora/render/MaterialBlock.hpp"

namespace yqlora {

const rhi::Buffer* GpuMaterialCache::block(const FrameMaterial& material)
{
    if (material.shader->materialSize == 0) {
        return nullptr;
    }

    return &m_blocks.get(material.id, {material.revision, material.shaderRevision}, [&] {
        const std::vector<std::byte> bytes = materialBlock(*material.shader, *material.data);

        return m_device.createBuffer({.size = bytes.size(), .usage = rhi::BufferUsage::Uniform}, bytes);
    });
}

} // namespace yqlora
