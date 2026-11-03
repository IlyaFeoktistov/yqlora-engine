#include "yqlora/shader/UniformLayout.hpp"

#include <algorithm>
#include <span>

namespace yqlora {

namespace {

// Поля шейдера fields против полей C++ members; prefix - путь поля в
// сообщении ("punctualLights[].").
void checkFields(
    const std::string& where,
    const std::string& prefix,
    std::span<const rhi::UniformField> fields,
    std::span<const UniformMember> members,
    std::vector<std::string>& errors
)
{
    for (const rhi::UniformField& field : fields) {
        const std::string name = prefix + field.name;
        const auto member = std::ranges::find(members, std::string_view(field.name), &UniformMember::name);
        if (member == members.end()) {
            errors.push_back(where + ": field '" + name + "' is not in the C++ struct");
            continue;
        }
        if (member->offset != field.offset) {
            errors.push_back(
                where + ": field '" + name + "' at offset " + std::to_string(field.offset) + " in the shader, "
                + std::to_string(member->offset) + " in C++"
            );
        }
        if (member->arrayStride != field.arrayStride) {
            errors.push_back(
                where + ": field '" + name + "' has an array stride of " + std::to_string(field.arrayStride)
                + " bytes in the shader, " + std::to_string(member->arrayStride) + " in C++"
            );
        }
        if (member->size != field.size) {
            errors.push_back(
                where + ": field '" + name + "' is " + std::to_string(field.size) + " bytes in the shader, "
                + std::to_string(member->size) + " in C++"
            );
        }
        if (field.members.empty()) {
            continue;
        }
        if (member->members.empty()) {
            errors.push_back(where + ": field '" + name + "' is a struct, its fields are not described in C++");
            continue;
        }
        const std::string inner = name + (field.arrayStride != 0 ? "[]." : ".");
        checkFields(where, inner, field.members, member->members, errors);
    }
}

} // namespace

std::vector<std::string> checkUniformLayout(const rhi::ShaderReflection& reflection, const UniformLayout& layout)
{
    const rhi::ShaderBinding* block = reflection.find(layout.slot);
    if (!block) {
        return {};
    }
    const std::string where = "uniform block '" + std::string(layout.name) + "'";
    if (block->kind != rhi::BindingKind::UniformBlock) {
        return {where + ": the shader has a texture at its set and binding"};
    }

    std::vector<std::string> errors;
    if (block->size > layout.size) {
        errors.push_back(
            where + ": " + std::to_string(block->size) + " bytes in the shader, " + std::to_string(layout.size)
            + " in C++"
        );
    }
    checkFields(where, {}, block->fields, layout.members, errors);

    return errors;
}

} // namespace yqlora
