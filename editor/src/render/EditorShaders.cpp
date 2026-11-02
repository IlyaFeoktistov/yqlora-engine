#include "yqlora/editor/render/EditorShaders.hpp"

#include "EmbeddedEditorShaders.hpp"

namespace yqlora::editor {

std::optional<std::string> readEditorShader(const std::string& path)
{
    if (!path.starts_with(kEditorShaderDir)) {
        return std::nullopt;
    }
    const std::string_view file = std::string_view(path).substr(kEditorShaderDir.size());
    for (const auto& [filePath, text] : shaders::kEmbeddedShaders) {
        if (filePath == file) {
            return std::string(text);
        }
    }

    return std::nullopt;
}

} // namespace yqlora::editor
