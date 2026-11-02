#pragma once

#include "yqlora/rhi/Pipeline.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace yqlora::opengl {

// Программа OpenGL из SPIR-V стадий (через GLSL 330). В OpenGL нет наборов:
// каждому ресурсу шейдера (набор, привязка) программа даёт свою точку
// привязки uniform-блока или текстурный блок, по порядку отражения.
class GlShader final : public rhi::Shader {
public:
    // Бросает std::runtime_error с логом компилятора, если шейдер не собрался.
    explicit GlShader(rhi::ShaderDesc desc);
    ~GlShader() override;

    unsigned int program() const
    {
        return m_program;
    }

    // Точка привязки uniform-блока или номер текстурного блока ресурса.
    std::optional<unsigned int> slot(rhi::BindingSlot binding) const;

private:
    struct Slot {
        rhi::BindingSlot binding{};
        unsigned int slot = 0;
    };

    unsigned int compile(unsigned int stage, const std::string& source) const;
    void assignSlots();

    unsigned int m_program = 0;
    std::vector<Slot> m_slots;
};

} // namespace yqlora::opengl
