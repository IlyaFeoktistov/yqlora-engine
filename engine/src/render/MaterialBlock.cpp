#include "yqlora/render/MaterialBlock.hpp"

#include "yqlora/math/Math.hpp"

#include <cstdint>
#include <cstring>
#include <span>

namespace yqlora {

namespace {

// RGB цвета - первые три компоненты, альфа (четвёртая) не переводится.
constexpr std::size_t kColorChannels = 3;

template <typename T>
void writeComponents(std::span<std::byte> block, std::uint32_t offset, std::span<const float> numbers)
{
    for (std::size_t i = 0; i < numbers.size(); ++i) {
        const T value = static_cast<T>(numbers[i]);
        std::memcpy(block.data() + offset + i * sizeof(T), &value, sizeof(T));
    }
}

void writeField(std::span<std::byte> block, const MaterialField& field, std::vector<float> numbers)
{
    if (field.color) {
        for (std::size_t i = 0; i < kColorChannels && i < numbers.size(); ++i) {
            numbers[i] = math::srgbToLinear(numbers[i]);
        }
    }
    switch (field.type) {
    case rhi::ValueType::Int:
    case rhi::ValueType::IVec2:
    case rhi::ValueType::IVec3:
    case rhi::ValueType::IVec4: writeComponents<std::int32_t>(block, field.offset, numbers); break;
    case rhi::ValueType::UInt: writeComponents<std::uint32_t>(block, field.offset, numbers); break;
    default: writeComponents<float>(block, field.offset, numbers); break;
    }
}

} // namespace

std::vector<std::byte> materialBlock(
    const ShaderData& shader,
    const MaterialData& material,
    const MaterialOverrides* overrides,
    const std::function<void(std::string_view)>& skipped
)
{
    std::vector<std::byte> block(shader.materialSize);
    for (const MaterialField& field : shader.fields) {
        const std::span<const float> value = materialValue(material, field);
        writeField(block, field, {value.begin(), value.end()});
    }
    if (!overrides) {
        return block;
    }
    for (const auto& [name, value] : overrides->values()) {
        const MaterialField* field = shader.findField(name);
        if (!field || value.numbers.size() != field->defaultValue.size() || value.color != field->color) {
            if (skipped) {
                skipped(name);
            }
            continue;
        }
        writeField(block, *field, value.numbers);
    }

    return block;
}

} // namespace yqlora
