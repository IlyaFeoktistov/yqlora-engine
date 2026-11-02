#pragma once

#include "yqlora/rhi/ShaderReflection.hpp"

#include <cstdint>
#include <span>
#include <string>

namespace yqlora::opengl {

// Имя ресурса (набор, привязка) в GLSL 330. В OpenGL 3.3 нет наборов и
// привязок в шейдере: программа находит ресурс по имени, и имя у него одно
// во всех стадиях.
std::string glResourceName(rhi::BindingSlot slot);

// SPIR-V стадии -> GLSL 330 через SPIRV-Cross. Глубина клипа переводится
// из 0..1 движка в -1..1 OpenGL (только вершинная стадия). Выходы и входы
// между стадиями получают имена по месту (location): у GLSL 330 нет
// layout(location) для них, стадии связываются по имени.
// Бросает std::runtime_error, если перевод не удался.
std::string translateToGlsl330(std::span<const std::uint32_t> spirv, rhi::ShaderStage stage);

} // namespace yqlora::opengl
