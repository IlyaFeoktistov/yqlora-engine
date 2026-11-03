#pragma once

#include <cstddef>
#include <cstdint>

namespace yqlora::rhi {

// Только форматы, которыми рендерер пользуется сейчас; новые добавляются вместе
// с тем, кому они нужны.
enum class Format {
    None, // нет вложения: проход без глубины
    Rgba8,
    Rgba8Srgb,
    Rgba16Float, // линейный HDR-цвет кадра
    R8,
    Depth24Stencil8,
};

// Форматы заднего буфера окна у любого устройства (Device::backBuffer):
// конвейеры, которые рисуют в окно, рисуют и в цели этих форматов.
inline constexpr Format kBackBufferColorFormat = Format::Rgba8;
inline constexpr Format kBackBufferDepthFormat = Format::Depth24Stencil8;

// Состояние текстуры. Смена - явная команда CommandList::transition: OpenGL её
// пропускает, Vulkan и D3D12 превращают в барьер.
enum class ResourceState {
    ColorTarget,
    DepthTarget,
    ShaderRead,
    Present,
};

enum class Filter {
    Linear,
    Nearest,
};

enum class Wrap {
    Repeat,
    Clamp,
    Mirror,
};

// Что делать с вложением в начале прохода.
enum class LoadOp {
    Clear,
    Load,
};

enum class CullMode {
    None,
    Back,
};

enum class BlendMode {
    Opaque,
    Alpha, // src * a + dst * (1 - a)
};

// Ограничения устройства, которые нужны рендереру.
struct DeviceLimits {
    // Смещение uniform-буфера в setUniformBuffer кратно этому числу.
    std::size_t uniformOffsetAlignment = 256;
};

// Ближайшее сверху кратное alignment (степень двойки не требуется).
constexpr std::size_t alignUp(std::size_t value, std::size_t alignment)
{
    return (value + alignment - 1) / alignment * alignment;
}

} // namespace yqlora::rhi
