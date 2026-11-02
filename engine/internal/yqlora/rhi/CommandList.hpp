#pragma once

#include "yqlora/math/Math.hpp"
#include "yqlora/rhi/Texture.hpp"

#include <cstddef>
#include <cstdint>

namespace yqlora::rhi {

class Buffer;
class Pipeline;

// Цели прохода. Вложения одного размера; глубины может не быть.
struct RenderPassDesc {
    Texture* color = nullptr;
    Texture* depth = nullptr;
    LoadOp colorLoad = LoadOp::Clear;
    LoadOp depthLoad = LoadOp::Clear;
    Vec4 clearColor{0.0f, 0.0f, 0.0f, 1.0f};
    float clearDepth = 1.0f;
};

// Команды кадра. Порядок вызовов:
//   transition целей -> beginRenderPass -> (bindPipeline -> ресурсы -> draw)* -> endRenderPass
// После bindPipeline все ресурсы задаются заново: у разных конвейеров разная
// раскладка привязок. Объекты, на которые ссылаются команды, должны жить до
// конца кадра (см. Device).
class CommandList {
public:
    virtual ~CommandList() = default;

    // Переводит текстуру в состояние state; в то же состояние - ничего не делает.
    // Вне прохода: вложения открытого прохода состояния не меняют.
    void transition(Texture& texture, ResourceState state)
    {
        if (texture.m_state == state) {
            return;
        }
        recordTransition(texture, texture.m_state, state);
        texture.m_state = state;
    }

    // Вложения - в состояниях ColorTarget и DepthTarget. Область вывода - вся цель.
    virtual void beginRenderPass(const RenderPassDesc& desc) = 0;
    virtual void endRenderPass() = 0;

    // Конвейер с форматами открытого прохода (checkPipelineTarget).
    virtual void bindPipeline(const Pipeline& pipeline) = 0;

    virtual void setVertexBuffer(const Buffer& buffer) = 0;
    virtual void setIndexBuffer(const Buffer& buffer) = 0;

    // offset кратно DeviceLimits::uniformOffsetAlignment.
    virtual void setUniformBuffer(
        std::uint32_t set,
        std::uint32_t binding,
        const Buffer& buffer,
        std::size_t offset,
        std::size_t size
    ) = 0;

    // Текстура в состоянии ShaderRead.
    virtual void setTexture(
        std::uint32_t set,
        std::uint32_t binding,
        const Texture& texture,
        const Sampler& sampler
    ) = 0;

    // Треугольники.
    virtual void draw(std::uint32_t vertexCount, std::uint32_t firstVertex = 0) = 0;
    virtual void drawIndexed(std::uint32_t indexCount, std::uint32_t firstIndex = 0) = 0;

protected:
    virtual void recordTransition(Texture& texture, ResourceState from, ResourceState to) = 0;
};

} // namespace yqlora::rhi
