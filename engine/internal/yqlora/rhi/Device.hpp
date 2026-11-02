#pragma once

#include "yqlora/rhi/Buffer.hpp"
#include "yqlora/rhi/CommandList.hpp"
#include "yqlora/rhi/Pipeline.hpp"
#include "yqlora/rhi/Texture.hpp"

#include <cstddef>
#include <memory>
#include <span>
#include <vector>

namespace yqlora::rhi {

// Слой над графическим API: создаёт объекты в видеопамяти и даёт команды кадра.
// Реализации живут в backends/<api>/.
//
// Время жизни: объектом владеет тот, кто его создал. Удалённый объект
// освобождается, когда кончатся кадры, которые могут им пользоваться: в
// OpenGL сразу, в Vulkan и D3D12 после кадров в полёте. Устройство удаляется
// последним, после всех своих объектов.
class Device {
public:
    virtual ~Device() = default;

    virtual const DeviceLimits& limits() const = 0;

    virtual std::unique_ptr<Buffer> createBuffer(const BufferDesc& desc, std::span<const std::byte> data = {}) = 0;
    // pixels - строки сверху вниз, плотно, без выравнивания строк.
    virtual std::unique_ptr<Texture> createTexture(const TextureDesc& desc, std::span<const std::byte> pixels = {}) = 0;
    virtual std::unique_ptr<Sampler> createSampler(const SamplerDesc& desc) = 0;
    // Бросает std::runtime_error, если шейдер не собрался.
    virtual std::unique_ptr<Shader> createShader(ShaderDesc desc) = 0;
    virtual std::unique_ptr<Pipeline> createPipeline(PipelineDesc desc) = 0;

    // Задний буфер окна и его глубина: цели прохода, как обычные текстуры.
    // Между кадрами задний буфер в состоянии Present.
    virtual Texture& backBuffer() = 0;
    virtual Texture& backBufferDepth() = 0;
    virtual void resizeBackBuffer(int width, int height) = 0;

    // Команды текущего кадра.
    virtual CommandList& commands() = 0;

    // Нарисованная картинка цели (текстура с kColorTarget, вне прохода):
    // строки сверху вниз, плотно, как в createTexture. Ждёт, пока видеокарта
    // дорисует, поэтому только для тестов и снимков экрана, не каждый кадр.
    virtual std::vector<std::byte> readPixels(Texture& texture) = 0;

    // Показывает задний буфер и начинает следующий кадр. Задний буфер
    // перед этим - в состоянии Present.
    virtual void present() = 0;

    // false, если система не даёт переключить.
    virtual bool setVsync(bool enabled) = 0;
};

} // namespace yqlora::rhi
