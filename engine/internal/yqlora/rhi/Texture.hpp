#pragma once

#include "yqlora/rhi/RhiTypes.hpp"

namespace yqlora::rhi {

class CommandList;

// Как текстурой пользуются, можно сочетать.
enum TextureUsage : unsigned {
    kSampled = 1U << 0U,
    kColorTarget = 1U << 1U,
    kDepthTarget = 1U << 2U,
};

struct TextureDesc {
    int width = 1;
    int height = 1;
    Format format = Format::Rgba8;
    unsigned usage = kSampled;
    bool mipmaps = false;
};

// Начальное состояние новой текстуры: цель, если она для рисования, иначе
// чтение в шейдере.
constexpr ResourceState initialState(const TextureDesc& desc)
{
    if ((desc.usage & kColorTarget) != 0U) {
        return ResourceState::ColorTarget;
    }
    if ((desc.usage & kDepthTarget) != 0U) {
        return ResourceState::DepthTarget;
    }

    return ResourceState::ShaderRead;
}

// Текстура в видеопамяти. Размер и формат неизменны: другой размер - новая
// текстура. Состояние меняет только CommandList::transition.
class Texture {
public:
    explicit Texture(const TextureDesc& desc)
        : m_desc(desc)
        , m_state(initialState(desc))
    {
    }

    virtual ~Texture() = default;

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    const TextureDesc& desc() const
    {
        return m_desc;
    }

    ResourceState state() const
    {
        return m_state;
    }

protected:
    // Задний буфер окна меняет размер вместе с окном, остальные текстуры нет.
    void setSize(int width, int height)
    {
        m_desc.width = width;
        m_desc.height = height;
    }

private:
    friend class CommandList;

    TextureDesc m_desc;
    ResourceState m_state;
};

struct SamplerDesc {
    Filter filter = Filter::Linear;
    Wrap wrap = Wrap::Repeat;
    // Читать ли мипмапы текстуры, если они есть.
    bool mipmaps = true;
};

// Как читать текстуру в шейдере. Отдельно от текстуры, как в Vulkan и D3D12.
class Sampler {
public:
    explicit Sampler(const SamplerDesc& desc)
        : m_desc(desc)
    {
    }

    virtual ~Sampler() = default;

    Sampler(const Sampler&) = delete;
    Sampler& operator=(const Sampler&) = delete;

    const SamplerDesc& desc() const
    {
        return m_desc;
    }

private:
    SamplerDesc m_desc;
};

} // namespace yqlora::rhi
