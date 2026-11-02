#include "yqlora/import/ShaderLoader.hpp"

#include "yqlora/core/Utf8Path.hpp"
#include "yqlora/render/EngineUniforms.hpp"
#include "yqlora/shader/EngineShaders.hpp"
#include "yqlora/shader/ShaderDataBuild.hpp"
#include "yqlora/shader/ShaderProfile.hpp"

#include <array>
#include <fstream>
#include <sstream>

namespace yqlora {

namespace fs = std::filesystem;

namespace {

constexpr std::array<std::string_view, 1> kExtensions{kShaderFileExtension};
constexpr char kProfileVersionKey[] = "profileVersion";

// Папка проекта по полному и относительному пути файла.
fs::path projectRoot(const LoadRequest& request)
{
    fs::path root = request.file;
    for (auto part = request.relative.begin(); part != request.relative.end(); ++part) {
        root = root.parent_path();
    }

    return root;
}

std::optional<std::string> readFile(const fs::path& file)
{
    std::ifstream stream(file, std::ios::binary);
    if (!stream) {
        return std::nullopt;
    }
    std::ostringstream text;
    text << stream.rdbuf();

    return text.str();
}

} // namespace

std::span<const std::string_view> ShaderLoader::extensions() const
{
    return kExtensions;
}

std::string_view ShaderLoader::kind() const
{
    return ShaderData::kResourceKind;
}

LoadResult ShaderLoader::load(const LoadRequest& request) const
{
    LoadResult result;
    result.importUpdate[kProfileVersionKey] = kShaderProfileVersion;
    if (const auto version = request.import.find(kProfileVersionKey);
        version != request.import.end() && *version != kShaderProfileVersion) {
        result.importUpdate = nlohmann::json::object();
        result.error = "the shader is written for shader profile " + version->dump() + ", the engine has "
                       + std::to_string(kShaderProfileVersion) + "; upgrade the project";

        return result;
    }

    // Пути внутри компилятора - от папки проекта: так их видно в ошибках, а
    // подключение не выходит за пределы проекта.
    const fs::path root = projectRoot(request);
    ShaderCompiler compiler([&](const std::string& path) -> std::optional<std::string> {
        const fs::path relative = fromUtf8(path).lexically_normal();
        if (relative.empty() || relative.is_absolute() || *relative.begin() == "..") {
            return std::nullopt;
        }

        return readFile(root / relative);
    });
    const std::string path = toGenericUtf8(request.relative);
    ShaderDataResult built = buildShaderData(compiler, path, toUtf8(request.relative.stem()), kEngineBlocks);
    for (const std::string& include : built.includes) {
        if (!include.starts_with(kEngineShaderScheme)) {
            result.dependencies.push_back(root / fromUtf8(include));
        }
    }
    if (!built.data) {
        result.error = "the shader does not compile:";
        for (const ShaderMessage& message : built.errors) {
            result.error += "\n" + formatShaderMessage(message);
        }

        return result;
    }
    result.resources.push_back(makeLoadedResource({}, std::move(*built.data)));

    return result;
}

} // namespace yqlora
