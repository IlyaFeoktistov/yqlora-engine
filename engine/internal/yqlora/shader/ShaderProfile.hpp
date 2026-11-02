#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/rhi/ShaderReflection.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace yqlora {

// Профиль шейдеров: то, что умеет каждый целевой API (см. rendering.md,
// раздел Shaders). Шейдер вне профиля отвергается при компиляции. Новая
// версия профиля - миграция файлов шейдеров проекта.
inline constexpr int kShaderProfileVersion = 1;

// Наборы ресурсов по частоте смены.
inline constexpr std::uint32_t kFrameSet = 0;
inline constexpr std::uint32_t kViewSet = 1;
inline constexpr std::uint32_t kMaterialSet = 2;
inline constexpr std::uint32_t kObjectSet = 3;

// Входы вершин по постоянным местам.
inline constexpr std::uint32_t kPositionLocation = 0;
inline constexpr std::uint32_t kNormalLocation = 1;
inline constexpr std::uint32_t kUv0Location = 2;
// Как TANGENT в glTF: vec4, w - знак бинормали.
inline constexpr std::uint32_t kTangentLocation = 3;

// Пределы OpenGL 3.3.
inline constexpr std::size_t kMaxShaderTextures = 16;
inline constexpr std::uint32_t kMaxUniformBlockSize = 16 * 1024;

// Ошибки отражения по одной на нарушение; пусто - шейдер в профиле. Ресурсы
// не из профиля (storage-буферы, image, отдельные сэмплеры) отвергает
// уже reflectShader: в отражении им нет места.
YQLORA_API std::vector<std::string> checkShaderProfile(const rhi::ShaderReflection& reflection);

// Отражение двух стадий SPIR-V вместе. Ресурс, объявленный в обеих стадиях
// по-разному, и ресурсы вне профиля - ошибки в errors.
struct ShaderReflectResult {
    rhi::ShaderReflection reflection{};
    std::vector<std::string> errors{};
};

YQLORA_API ShaderReflectResult
reflectShader(std::span<const std::uint32_t> vertex, std::span<const std::uint32_t> fragment);

} // namespace yqlora
