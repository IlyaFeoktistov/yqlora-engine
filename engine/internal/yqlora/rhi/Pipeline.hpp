#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/rhi/RhiTypes.hpp"
#include "yqlora/rhi/ShaderReflection.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace yqlora::rhi {

// Шейдер в SPIR-V: вершинная и фрагментная стадии и их отражение. Бэкенд
// переводит SPIR-V в свой язык (OpenGL - GLSL 330 через SPIRV-Cross).
struct ShaderDesc {
    std::string name{};
    std::vector<std::uint32_t> vertex{};
    std::vector<std::uint32_t> fragment{};
    ShaderReflection reflection{};
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
    const ShaderBinding* findBinding(BindingSlot slot) const
    {
        return m_desc.reflection.find(slot);
    }

private:
    ShaderDesc m_desc;
};

enum class VertexFormat {
    Float2,
    Float3,
    Float4,
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
    // Конвейер рисует только в цель с такими же форматами и числом образцов;
    // по умолчанию - в окно.
    Format colorFormat = kBackBufferColorFormat;
    Format depthFormat = kBackBufferDepthFormat;
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
