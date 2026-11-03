#include "yqlora/editor/ResourceLabel.hpp"

#include "yqlora/core/Utf8Path.hpp"
#include "yqlora/i18n/Translate.hpp"
#include "yqlora/resource/BuiltinResourceTable.hpp"
#include "yqlora/resource/ResourceDatabase.hpp"
#include "yqlora/resource/ResourceLoader.hpp"

namespace yqlora::editor {

std::string resourceLabel(ResourceRef value, const ResourceDatabase* resources)
{
    if (value.id == 0) {
        return tr("editor.inspector.none");
    }
    if (const BuiltinResourceEntry* builtin = findBuiltinResource(value.id)) {
        return std::string(builtin->name);
    }
    if (const auto path = resources ? resources->path(value.id) : std::nullopt) {
        const std::string_view key = resources->key(value.id);
        if (key.empty()) {
            return toGenericUtf8(*path);
        }

        return toGenericUtf8(*path) + " / " + std::string(subresourceLocalKey(key));
    }

    return tr("editor.inspector.missing");
}

} // namespace yqlora::editor
