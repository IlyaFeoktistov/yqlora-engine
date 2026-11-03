#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/reflection/EnumNames.hpp"
#include "yqlora/resource/BuiltinResources.hpp"
#include "yqlora/resource/ResourceId.hpp"
#include "yqlora/resource/ResourceKinds.hpp"

#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace yqlora {

struct MaterialField;
struct MaterialTexture;

// Как материал смешивается с тем, что уже нарисовано.
enum class MaterialBlend : std::uint8_t {
    // Тест и запись глубины, без смешивания.
    Opaque,
    // Смешивание по альфе в линейном RGB, тест глубины без записи; рисуется
    // после непрозрачного, сзади вперёд.
    Transparent,
};

// Какие грани не рисуются.
enum class MaterialCull : std::uint8_t {
    Back,
    None,
};

} // namespace yqlora

YQLORA_ENUM_NAMES(yqlora::MaterialBlend, "opaque", "transparent")
YQLORA_ENUM_NAMES(yqlora::MaterialCull, "back", "none")

namespace yqlora {

// Ресурс материала (.ymat и встроенные): шейдер, состояние отрисовки,
// значения полей блока Material и текстуры по их именам. Значения, которых
// у шейдера нет, хранятся и пишутся обратно: вернётся поле - вернётся и
// значение. Цвета (поля с пометкой color) - в sRGB, как их выбирает человек.
struct MaterialData {
    static constexpr std::string_view kResourceKind = kinds::kMaterial;

    ResourceId shader = builtin::kStandardShader;
    MaterialBlend blend = MaterialBlend::Opaque;
    MaterialCull cull = MaterialCull::Back;
    // Числа поля по компонентам (float - одно, vec4 - четыре).
    std::map<std::string, std::vector<float>> values{};
    std::map<std::string, ResourceId> textures{};

    bool operator==(const MaterialData&) const = default;
};

// Значение поля для шейдера: материала, если оно есть и того же числа
// компонент, иначе default поля.
YQLORA_API std::span<const float> materialValue(const MaterialData& material, const MaterialField& field);

// Текстура для шейдера: материала; 0 - не задана, тогда текстура по
// умолчанию (MaterialTexture::defaultTexture).
YQLORA_API ResourceId materialTexture(const MaterialData& material, const MaterialTexture& texture);

} // namespace yqlora
