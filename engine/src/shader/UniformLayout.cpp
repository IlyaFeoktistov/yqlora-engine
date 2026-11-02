#include "yqlora/shader/UniformLayout.hpp"

#include <algorithm>

namespace yqlora {

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
    for (const rhi::UniformField& field : block->fields) {
        const auto member = std::ranges::find(layout.members, std::string_view(field.name), &UniformMember::name);
        if (member == layout.members.end()) {
            errors.push_back(where + ": field '" + field.name + "' is not in the C++ struct");
            continue;
        }
        if (member->offset != field.offset) {
            errors.push_back(
                where + ": field '" + field.name + "' at offset " + std::to_string(field.offset) + " in the shader, "
                + std::to_string(member->offset) + " in C++"
            );
        }
        if (member->arrayStride != field.arrayStride) {
            errors.push_back(
                where + ": field '" + field.name + "' has an array stride of " + std::to_string(field.arrayStride)
                + " bytes in the shader, " + std::to_string(member->arrayStride) + " in C++"
            );
        }
        if (member->size != field.size) {
            errors.push_back(
                where + ": field '" + field.name + "' is " + std::to_string(field.size) + " bytes in the shader, "
                + std::to_string(member->size) + " in C++"
            );
        }
    }

    return errors;
}

} // namespace yqlora
