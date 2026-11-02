#include "yqlora/shader/EngineShaders.hpp"

#include "EmbeddedShaders.hpp"

namespace yqlora {

std::optional<std::string_view> engineShaderFile(std::string_view path)
{
    for (const auto& [filePath, text] : shaders::kEmbeddedShaders) {
        if (filePath == path) {
            return text;
        }
    }

    return std::nullopt;
}

} // namespace yqlora
