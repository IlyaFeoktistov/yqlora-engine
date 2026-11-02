#include "yqlora/opengl/GlShader.hpp"

#include <glad/gl.h>

#include <array>
#include <stdexcept>

namespace yqlora::opengl {

namespace {

constexpr std::size_t kLogSize = 1024;

} // namespace

GlShader::GlShader(rhi::ShaderDesc desc)
    : rhi::Shader(std::move(desc))
{
    const std::string& name = this->desc().name;
    const GLuint vertex = compile(GL_VERTEX_SHADER, this->desc().vertex);
    GLuint fragment = 0;
    try {
        fragment = compile(GL_FRAGMENT_SHADER, this->desc().fragment);
    } catch (...) {
        glDeleteShader(vertex);
        throw;
    }

    m_program = glCreateProgram();
    glAttachShader(m_program, vertex);
    glAttachShader(m_program, fragment);
    glLinkProgram(m_program);
    // После линковки шейдеры программе больше не нужны.
    glDeleteShader(vertex);
    glDeleteShader(fragment);

    GLint linked = GL_FALSE;
    glGetProgramiv(m_program, GL_LINK_STATUS, &linked);
    if (!linked) {
        std::array<char, kLogSize> log{};
        glGetProgramInfoLog(m_program, static_cast<GLsizei>(log.size()), nullptr, log.data());
        glDeleteProgram(m_program);
        throw std::runtime_error("Shader '" + name + "' link failed: " + log.data());
    }
    assignSlots();
}

GlShader::~GlShader()
{
    glDeleteProgram(m_program);
}

std::optional<unsigned int> GlShader::slot(std::uint32_t set, std::uint32_t binding) const
{
    for (const Slot& entry : m_slots) {
        if (entry.set == set && entry.binding == binding) {
            return entry.slot;
        }
    }

    return std::nullopt;
}

void GlShader::assignSlots()
{
    unsigned int nextBlock = 0;
    unsigned int nextUnit = 0;
    glUseProgram(m_program);
    for (const rhi::ShaderBinding& binding : desc().bindings) {
        // Ресурс, который компилятор выбросил как неиспользуемый, программе
        // не найти; точка за ним всё равно числится, привязка к ней безвредна.
        if (binding.kind == rhi::BindingKind::UniformBlock) {
            const GLuint index = glGetUniformBlockIndex(m_program, binding.name.c_str());
            if (index != GL_INVALID_INDEX) {
                glUniformBlockBinding(m_program, index, nextBlock);
            }
            m_slots.push_back({binding.set, binding.binding, nextBlock++});
        } else {
            const GLint location = glGetUniformLocation(m_program, binding.name.c_str());
            if (location >= 0) {
                glUniform1i(location, static_cast<GLint>(nextUnit));
            }
            m_slots.push_back({binding.set, binding.binding, nextUnit++});
        }
    }
    glUseProgram(0);
}

unsigned int GlShader::compile(unsigned int stage, const std::string& source) const
{
    const GLuint shader = glCreateShader(stage);
    const char* text = source.c_str();
    glShaderSource(shader, 1, &text, nullptr);
    glCompileShader(shader);

    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        std::array<char, kLogSize> log{};
        glGetShaderInfoLog(shader, static_cast<GLsizei>(log.size()), nullptr, log.data());
        glDeleteShader(shader);
        const char* stageName = stage == GL_VERTEX_SHADER ? "vertex" : "fragment";
        throw std::runtime_error("Shader '" + desc().name + "' " + stageName + " stage failed: " + log.data());
    }

    return shader;
}

} // namespace yqlora::opengl
