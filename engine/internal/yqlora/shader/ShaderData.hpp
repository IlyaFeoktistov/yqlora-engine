#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/rhi/Pipeline.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace yqlora {

// Расширение файла шейдера проекта: обе стадии в одном файле.
inline constexpr std::string_view kShaderFileExtension = ".yshader";

// Чем стоит текстура материала, пока её не выбрали.
enum class DefaultTexture : std::uint8_t {
    White,
    Black,
    // (0.5, 0.5, 1): нормаль, не меняющая поверхность.
    Normal,
};

// Поле блока Material: место из отражения, вид в инспекторе из пометок.
struct MaterialField {
    std::string name{};
    rhi::ValueType type = rhi::ValueType::Other;
    std::uint32_t offset = 0;
    std::uint32_t size = 0;
    // Выбор цвета; значение в sRGB, шейдер получает линейное (альфа как есть).
    bool color = false;
    std::optional<float> min{};
    std::optional<float> max{};
    // По числу компонент типа; без default - нули.
    std::vector<float> defaultValue{};
    std::string label{};
    bool hidden = false;
};

// Текстура набора материала.
struct MaterialTexture {
    std::string name{};
    rhi::BindingSlot slot{};
    rhi::TextureDimension dimension = rhi::TextureDimension::Texture2D;
    DefaultTexture defaultTexture = DefaultTexture::White;
    std::string label{};
    bool hidden = false;
};

// Ресурс шейдера (.yshader и встроенные): готовое для устройства описание
// и раскладка материала. Материал задаёт значения по именам полей. Собирает
// его buildShaderData (ShaderDataBuild.hpp).
struct ShaderData {
    static constexpr std::string_view kResourceKind = "shader";

    rhi::ShaderDesc desc{};
    // Размер блока Material; 0 - блока нет.
    std::uint32_t materialSize = 0;
    // В порядке объявления в шейдере.
    std::vector<MaterialField> fields{};
    std::vector<MaterialTexture> textures{};

    YQLORA_API const MaterialField* findField(std::string_view name) const;
    YQLORA_API const MaterialTexture* findTexture(std::string_view name) const;
};

} // namespace yqlora
