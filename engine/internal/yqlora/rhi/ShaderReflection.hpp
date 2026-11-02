#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace yqlora::rhi {

enum class ShaderStage : std::uint8_t {
    Vertex = 1U << 0U,
    Fragment = 1U << 1U,
};

using ShaderStages = std::uint8_t;

constexpr ShaderStages stageBit(ShaderStage stage)
{
    return static_cast<ShaderStages>(stage);
}

// Место ресурса шейдера: набор и привязка, как в Vulkan.
struct BindingSlot {
    std::uint32_t set = 0;
    std::uint32_t binding = 0;

    bool operator==(const BindingSlot&) const = default;
};

// Типы полей uniform-блоков и входов вершин, которые есть в профиле шейдеров.
// Остальное (структуры, массивы структур, double) - Other: такое поле видно,
// но материал его не покажет.
enum class ValueType {
    Float,
    Vec2,
    Vec3,
    Vec4,
    Int,
    IVec2,
    IVec3,
    IVec4,
    UInt,
    Mat3,
    Mat4,
    Other,
};

struct UniformField {
    std::string name{};
    ValueType type = ValueType::Other;
    std::uint32_t offset = 0;
    // Байт в раскладке std140, с учётом массива.
    std::uint32_t size = 0;
    // 0 - не массив.
    std::uint32_t arraySize = 0;
    // Байт от элемента до элемента массива; 0 - не массив.
    std::uint32_t arrayStride = 0;
};

enum class BindingKind {
    UniformBlock,
    Texture, // текстура вместе с сэмплером (sampler2D, samplerCube)
};

enum class TextureDimension {
    Texture2D,
    Cube,
};

// Ресурс шейдера, как его видит отражение SPIR-V.
struct ShaderBinding {
    // Имя блока (не экземпляра) или имя сэмплера.
    std::string name{};
    BindingKind kind = BindingKind::UniformBlock;
    BindingSlot slot{};
    ShaderStages stages = 0;
    // Только у блока.
    std::uint32_t size = 0;
    std::vector<UniformField> fields{};
    // Только у текстуры.
    TextureDimension dimension = TextureDimension::Texture2D;
};

struct VertexInput {
    std::string name{};
    std::uint32_t location = 0;
    ValueType type = ValueType::Other;
};

// Устройство шейдера: ресурсы обеих стадий вместе и входы вершин. Источник -
// SPIR-V, руками не пишется.
struct ShaderReflection {
    std::vector<ShaderBinding> bindings{};
    std::vector<VertexInput> vertexInputs{};

    // nullptr, если такого ресурса нет.
    const ShaderBinding* find(BindingSlot slot) const
    {
        for (const ShaderBinding& entry : bindings) {
            if (entry.slot == slot) {
                return &entry;
            }
        }

        return nullptr;
    }
};

} // namespace yqlora::rhi
