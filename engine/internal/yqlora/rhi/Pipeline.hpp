#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/rhi/RhiTypes.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace yqlora::rhi {

enum class BindingKind {
    UniformBlock,
    Texture, // текстура вместе с сэмплером (sampler2D)
};

// Ресурс шейдера по набору и привязке, как в Vulkan. Имя - то, что в шейдере:
// по нему OpenGL находит ресурс в программе.
struct ShaderBinding {
    std::string name{};
    BindingKind kind = BindingKind::UniformBlock;
    std::uint32_t set = 0;
    std::uint32_t binding = 0;
};

// Исходники стадий и ресурсы шейдера. Сейчас это GLSL 330 для OpenGL и список
// ресурсов вручную; с R.2 здесь SPIR-V, а ресурсы даёт отражение.
struct ShaderDesc {
    std::string name{};
    std::string vertex{};
    std::string fragment{};
    std::vector<ShaderBinding> bindings{};
};

class Shader {
public:
    explicit Shader(ShaderDesc desc)
        : m_desc(std::move(desc))
    {
    }

    virtual ~Shader() = default;

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    const ShaderDesc& desc() const
    {
        return m_desc;
    }

    // nullptr, если такого ресурса у шейдера нет.
    const ShaderBinding* findBinding(std::uint32_t set, std::uint32_t binding) const
    {
        for (const ShaderBinding& entry : m_desc.bindings) {
            if (entry.set == set && entry.binding == binding) {
                return &entry;
            }
        }

        return nullptr;
    }

private:
    ShaderDesc m_desc;
};

enum class VertexFormat {
    Float2,
    Float3,
};

struct VertexAttribute {
    std::uint32_t location = 0;
    VertexFormat format = VertexFormat::Float3;
    std::size_t offset = 0;
};

// Пустая раскладка - вершины без буфера, их строит сам шейдер (gl_VertexID).
struct VertexLayout {
    std::size_t stride = 0;
    std::vector<VertexAttribute> attributes{};
};

// Всё состояние отрисовки одним объектом, неизменным после создания.
struct PipelineDesc {
    const Shader* shader = nullptr;
    VertexLayout vertexLayout{};
    bool depthTest = true;
    bool depthWrite = true;
    CullMode cull = CullMode::Back;
    BlendMode blend = BlendMode::Opaque;
    // Конвейер рисует только в цель с такими же форматами и числом образцов.
    Format colorFormat = Format::Rgba8;
    Format depthFormat = Format::Depth24Stencil8;
    int samples = 1;
};

class Pipeline {
public:
    explicit Pipeline(PipelineDesc desc)
        : m_desc(std::move(desc))
    {
    }

    virtual ~Pipeline() = default;

    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;

    const PipelineDesc& desc() const
    {
        return m_desc;
    }

private:
    PipelineDesc m_desc;
};

// Форматы вложений открытого прохода.
struct TargetFormats {
    Format color = Format::None;
    Format depth = Format::None;
    int samples = 1;
};

// Бросает std::logic_error, если конвейер не рисует в цель с такими форматами.
// Одна проверка на все бэкенды.
YQLORA_API void checkPipelineTarget(const PipelineDesc& pipeline, const TargetFormats& target);

} // namespace yqlora::rhi
