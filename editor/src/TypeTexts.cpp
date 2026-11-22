#include "yqlora/editor/TypeTexts.hpp"

#include "yqlora/core/Utf8Path.hpp"
#include "yqlora/editor/ScriptComment.hpp"
#include "yqlora/i18n/Translate.hpp"
#include "yqlora/reflection/TypeRegistry.hpp"

namespace yqlora::editor {

std::string typeDisplayName(const TypeInfo& type)
{
    return trOr(type.nameKey(), type.name());
}

std::string typeDisplayName(const TypeRegistry& registry, const std::string& typeName)
{
    const TypeInfo* type = registry.find(typeName);

    return type ? typeDisplayName(*type) : typeName;
}

std::string typeDescription(const TypeInfo& type, const std::filesystem::path& projectDirectory)
{
    if (std::string translated = trOr(type.descriptionKey(), {}); !translated.empty()) {
        return translated;
    }
    if (!type.description().empty()) {
        return type.description();
    }
    if (type.isScript() && !type.sourceFile().empty()) {
        return scriptComment(scriptSourcePath(type, projectDirectory), type.name());
    }

    return {};
}

std::filesystem::path scriptSourcePath(const TypeInfo& type, const std::filesystem::path& projectDirectory)
{
    if (type.sourceFile().empty()) {
        return {};
    }
    const std::filesystem::path source = fromUtf8(type.sourceFile());

    return source.is_absolute() ? source : projectDirectory / source;
}

} // namespace yqlora::editor
