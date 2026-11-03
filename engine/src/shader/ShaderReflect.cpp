#include "yqlora/shader/ShaderProfile.hpp"

#include <spirv_cross.hpp>

#include <algorithm>
#include <utility>

namespace yqlora {

namespace {

using spirv_cross::SPIRType;

rhi::ValueType valueType(const SPIRType& type)
{
    const bool isFloat = type.basetype == SPIRType::Float;
    if (isFloat && type.columns == 3 && type.vecsize == 3) {
        return rhi::ValueType::Mat3;
    }
    if (isFloat && type.columns == 4 && type.vecsize == 4) {
        return rhi::ValueType::Mat4;
    }
    if (type.columns != 1) {
        return rhi::ValueType::Other;
    }
    constexpr rhi::ValueType kFloats[] = {
        rhi::ValueType::Float,
        rhi::ValueType::Vec2,
        rhi::ValueType::Vec3,
        rhi::ValueType::Vec4,
    };
    constexpr rhi::ValueType kInts[] = {
        rhi::ValueType::Int,
        rhi::ValueType::IVec2,
        rhi::ValueType::IVec3,
        rhi::ValueType::IVec4,
    };
    const std::size_t index = type.vecsize - 1;
    if (isFloat) {
        return kFloats[index];
    }
    if (type.basetype == SPIRType::Int) {
        return kInts[index];
    }
    if (type.basetype == SPIRType::UInt && type.vecsize == 1) {
        return rhi::ValueType::UInt;
    }

    return rhi::ValueType::Other;
}

rhi::BindingSlot slotOf(const spirv_cross::Compiler& compiler, spirv_cross::ID id)
{
    return {
        compiler.get_decoration(id, spv::DecorationDescriptorSet),
        compiler.get_decoration(id, spv::DecorationBinding),
    };
}

// Поля структуры structId; у поля-структуры (и массива структур) - её поля.
std::vector<rhi::UniformField> structFields(const spirv_cross::Compiler& compiler, spirv_cross::TypeID structId)
{
    const SPIRType& type = compiler.get_type(structId);
    std::vector<rhi::UniformField> fields;
    for (std::uint32_t i = 0; i < type.member_types.size(); ++i) {
        const SPIRType& member = compiler.get_type(type.member_types[i]);
        const bool isStruct = member.basetype == SPIRType::Struct;
        fields.push_back({
            .name = compiler.get_member_name(structId, i),
            .type = isStruct ? rhi::ValueType::Other : valueType(member),
            .offset = compiler.type_struct_member_offset(type, i),
            .size = static_cast<std::uint32_t>(compiler.get_declared_struct_member_size(type, i)),
            .arraySize = member.array.empty() ? 0U : member.array.front(),
            .arrayStride = member.array.empty() ? 0U : compiler.type_struct_member_array_stride(type, i),
            // Имена и смещения полей лежат на самом типе структуры, не на массиве из неё.
            .members = isStruct ? structFields(compiler, member.self) : std::vector<rhi::UniformField>{},
        });
    }

    return fields;
}

rhi::ShaderBinding uniformBlock(const spirv_cross::Compiler& compiler, const spirv_cross::Resource& resource)
{
    const SPIRType& type = compiler.get_type(resource.base_type_id);

    return {
        .name = compiler.get_name(resource.base_type_id),
        .kind = rhi::BindingKind::UniformBlock,
        .slot = slotOf(compiler, resource.id),
        .size = static_cast<std::uint32_t>(compiler.get_declared_struct_size(type)),
        .fields = structFields(compiler, resource.base_type_id),
    };
}

// Ресурсы одной стадии; то, чего нет в профиле, - в errors.
void reflectStage(
    std::span<const std::uint32_t> spirv,
    rhi::ShaderStage stage,
    std::vector<rhi::ShaderBinding>& bindings,
    std::vector<rhi::VertexInput>& inputs,
    std::vector<std::string>& errors
)
{
    const spirv_cross::Compiler compiler(spirv.data(), spirv.size());
    // Только то, чем стадия пользуется: подключённый, но не прочитанный блок
    // glslang оставляет в SPIR-V, а задавать его перед отрисовкой незачем.
    const spirv_cross::ShaderResources resources = compiler.get_shader_resources(
        compiler.get_active_interface_variables()
    );

    const auto reject = [&](const spirv_cross::SmallVector<spirv_cross::Resource>& list, const char* what) {
        for (const spirv_cross::Resource& resource : list) {
            errors.push_back(std::string(what) + " '" + resource.name + "' is outside the shader profile");
        }
    };
    reject(resources.storage_buffers, "storage buffer");
    reject(resources.storage_images, "image");
    reject(resources.separate_images, "separate texture");
    reject(resources.separate_samplers, "separate sampler");
    reject(resources.push_constant_buffers, "push constant block");
    reject(resources.atomic_counters, "atomic counter");
    reject(resources.subpass_inputs, "subpass input");
    reject(resources.acceleration_structures, "acceleration structure");

    for (const spirv_cross::Resource& resource : resources.uniform_buffers) {
        rhi::ShaderBinding binding = uniformBlock(compiler, resource);
        binding.stages = rhi::stageBit(stage);
        bindings.push_back(std::move(binding));
    }
    for (const spirv_cross::Resource& resource : resources.sampled_images) {
        const SPIRType& type = compiler.get_type(resource.type_id);
        if (!type.array.empty()) {
            errors.push_back("texture array '" + resource.name + "' is outside the shader profile");
            continue;
        }
        if (type.image.dim != spv::Dim2D && type.image.dim != spv::DimCube) {
            errors.push_back("texture '" + resource.name + "': only 2D and cube textures are in the shader profile");
            continue;
        }
        bindings.push_back({
            .name = resource.name,
            .kind = rhi::BindingKind::Texture,
            .slot = slotOf(compiler, resource.id),
            .stages = rhi::stageBit(stage),
            .dimension = type.image.dim == spv::DimCube ? rhi::TextureDimension::Cube
                                                        : rhi::TextureDimension::Texture2D,
        });
    }
    if (stage == rhi::ShaderStage::Vertex) {
        for (const spirv_cross::Resource& resource : resources.stage_inputs) {
            inputs.push_back({
                .name = resource.name,
                .location = compiler.get_decoration(resource.id, spv::DecorationLocation),
                .type = valueType(compiler.get_type(resource.type_id)),
            });
        }
    }
}

bool sameResource(const rhi::ShaderBinding& a, const rhi::ShaderBinding& b)
{
    if (a.kind != b.kind || a.name != b.name) {
        return false;
    }
    if (a.kind == rhi::BindingKind::Texture) {
        return a.dimension == b.dimension;
    }
    if (a.size != b.size || a.fields.size() != b.fields.size()) {
        return false;
    }

    return a.fields == b.fields;
}

} // namespace

ShaderReflectResult reflectShader(std::span<const std::uint32_t> vertex, std::span<const std::uint32_t> fragment)
{
    ShaderReflectResult result;
    std::vector<rhi::ShaderBinding> fragmentBindings;
    try {
        reflectStage(
            vertex,
            rhi::ShaderStage::Vertex,
            result.reflection.bindings,
            result.reflection.vertexInputs,
            result.errors
        );
        reflectStage(
            fragment,
            rhi::ShaderStage::Fragment,
            fragmentBindings,
            result.reflection.vertexInputs,
            result.errors
        );
    } catch (const spirv_cross::CompilerError& error) {
        result.errors.emplace_back(std::string("SPIR-V reflection failed: ") + error.what());

        return result;
    }

    // Ресурс обеих стадий - одна запись с двумя стадиями.
    for (rhi::ShaderBinding& binding : fragmentBindings) {
        const auto existing = std::ranges::find(result.reflection.bindings, binding.slot, &rhi::ShaderBinding::slot);
        if (existing == result.reflection.bindings.end()) {
            result.reflection.bindings.push_back(std::move(binding));
        } else if (sameResource(*existing, binding)) {
            existing->stages |= binding.stages;
        } else {
            result.errors.push_back(
                "set " + std::to_string(binding.slot.set) + ", binding " + std::to_string(binding.slot.binding)
                + " is declared differently in the vertex and fragment stages"
            );
        }
    }

    return result;
}

std::vector<std::string> checkShaderProfile(const rhi::ShaderReflection& reflection)
{
    std::vector<std::string> errors;
    std::size_t textures = 0;
    for (const rhi::ShaderBinding& binding : reflection.bindings) {
        if (binding.slot.set > kObjectSet) {
            errors.push_back(
                "'" + binding.name + "': set " + std::to_string(binding.slot.set)
                + " is outside the shader profile (0.." + std::to_string(kObjectSet) + ")"
            );
        }
        if (binding.kind == rhi::BindingKind::Texture) {
            ++textures;
        } else if (binding.size > kMaxUniformBlockSize) {
            errors.push_back(
                "uniform block '" + binding.name + "' is " + std::to_string(binding.size) + " bytes, at most "
                + std::to_string(kMaxUniformBlockSize) + " are allowed"
            );
        }
    }
    if (textures > kMaxShaderTextures) {
        errors.push_back(
            std::to_string(textures) + " textures, at most " + std::to_string(kMaxShaderTextures) + " are allowed"
        );
    }
    for (const rhi::VertexInput& input : reflection.vertexInputs) {
        if (input.location > kTangentLocation) {
            errors.push_back(
                "vertex input '" + input.name + "': location " + std::to_string(input.location)
                + " is outside the shader profile (0 position, 1 normal, 2 uv0, 3 tangent)"
            );
        }
    }

    return errors;
}

} // namespace yqlora
