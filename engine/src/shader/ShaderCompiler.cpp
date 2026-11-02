#include "yqlora/shader/ShaderCompiler.hpp"

#include "yqlora/shader/EngineShaders.hpp"

#include <SPIRV/GlslangToSpv.h>
#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>

#include <algorithm>
#include <filesystem>
#include <mutex>
#include <regex>
#include <sstream>
#include <utility>

namespace yqlora {

namespace {

// Версия GLSL исходников и окружение Vulkan: наборы и привязки в шейдерах.
constexpr int kGlslVersion = 450;
constexpr int kVulkanInputVersion = 100;
// Без него glslang не понимает #include.
constexpr std::string_view kIncludeExtension = "#extension GL_GOOGLE_include_directive : require\n";
// glslang останавливается на такой глубине вложенных #include.
constexpr std::size_t kMaxIncludeDepth = 32;

// glslang требует один вызов InitializeProcess на процесс до первого шейдера
// и FinalizeProcess после последнего: счётчик живых компиляторов.
class GlslangProcess {
public:
    static void acquire()
    {
        const std::scoped_lock lock(state().mutex);
        if (state().users++ == 0) {
            glslang::InitializeProcess();
        }
    }

    static void release()
    {
        const std::scoped_lock lock(state().mutex);
        if (--state().users == 0) {
            glslang::FinalizeProcess();
        }
    }

private:
    struct State {
        std::mutex mutex;
        int users = 0;
    };

    static State& state()
    {
        static State instance;

        return instance;
    }
};

EShLanguage glslangStage(rhi::ShaderStage stage)
{
    return stage == rhi::ShaderStage::Vertex ? EShLangVertex : EShLangFragment;
}

bool isEnginePath(std::string_view path)
{
    return path.starts_with(kEngineShaderScheme);
}

// Путь header относительно папки файла includer, без "." и "..".
std::string siblingPath(const std::string& includer, const std::string& header)
{
    const std::string_view scheme = isEnginePath(includer) ? kEngineShaderScheme : std::string_view();
    const std::filesystem::path base = std::filesystem::path(includer.substr(scheme.size())).parent_path();

    return std::string(scheme) + (base / header).lexically_normal().generic_string();
}

struct IncludedFile {
    std::string path;
    std::string text;
};

// Файлы движка - из библиотеки, остальные - через reader проекта.
std::optional<std::string> readShaderFile(const ShaderFileReader& reader, const std::string& path)
{
    if (isEnginePath(path)) {
        if (const auto file = engineShaderFile(std::string_view(path).substr(kEngineShaderScheme.size()))) {
            return std::string(*file);
        }

        return std::nullopt;
    }
    if (!reader) {
        return std::nullopt;
    }

    return reader(path);
}

class Includer final : public glslang::TShader::Includer {
public:
    explicit Includer(const ShaderFileReader& reader)
        : m_reader(reader)
    {
    }

    IncludeResult* includeSystem(const char* header, const char* /*includer*/, size_t depth) override
    {
        return include(engineShaderPath(std::string(kEngineShaderIncludeDir) + header), depth);
    }

    IncludeResult* includeLocal(const char* header, const char* includer, size_t depth) override
    {
        return include(siblingPath(includer, header), depth);
    }

    void releaseInclude(IncludeResult* result) override
    {
        if (result) {
            delete static_cast<std::string*>(result->userData);
            delete result;
        }
    }

    const std::vector<IncludedFile>& included() const
    {
        return m_included;
    }

private:
    IncludeResult* include(const std::string& path, size_t depth)
    {
        if (depth > kMaxIncludeDepth) {
            return nullptr;
        }
        std::optional<std::string> text = readShaderFile(m_reader, path);
        if (!text) {
            return nullptr;
        }
        m_included.push_back({path, *text});
        auto* owned = new std::string(std::move(*text));

        return new IncludeResult(path, owned->data(), owned->size(), owned);
    }

    const ShaderFileReader& m_reader;
    std::vector<IncludedFile> m_included;
};

// Строки лога glslang: "ERROR: <файл>:<строка>: <текст>". В пути может быть
// двоеточие (C:\..., engine:), поэтому файл - всё до первого ":<число>: ".
std::vector<ShaderMessage> parseLog(const std::string& log, const std::string& path)
{
    static const std::regex location(R"(:(\d+): )");
    constexpr std::string_view kError = "ERROR: ";

    std::vector<ShaderMessage> messages;
    std::istringstream lines(log);
    std::string line;
    while (std::getline(lines, line)) {
        if (!line.starts_with(kError)) {
            continue;
        }
        const std::string body = line.substr(kError.size());
        // Итог в конце лога ("N compilation errors.  No code generated.") ничего не добавляет.
        if (body.find("compilation errors") != std::string::npos) {
            continue;
        }
        std::smatch match;
        if (std::regex_search(body, match, location)) {
            messages.push_back({
                .file = match.prefix().str(),
                .line = std::stoi(match[1].str()),
                .text = match.suffix().str(),
            });
        } else {
            messages.push_back({.file = path, .line = 0, .text = body});
        }
    }
    if (messages.empty()) {
        messages.push_back({.file = path, .line = 0, .text = log.empty() ? "compilation failed" : log});
    }

    return messages;
}

std::string preamble(const std::vector<ShaderDefine>& defines)
{
    std::string text(kIncludeExtension);
    for (const ShaderDefine& define : defines) {
        text += "#define " + define.name + " " + define.value + "\n";
    }

    return text;
}

} // namespace

std::string formatShaderMessage(const ShaderMessage& message)
{
    if (message.line > 0) {
        return message.file + ":" + std::to_string(message.line) + ": " + message.text;
    }

    return message.file + ": " + message.text;
}

struct ShaderCompiler::CacheEntry {
    ShaderCompileRequest request;
    std::string source;
    std::vector<IncludedFile> includes;
    ShaderCompileResult result;
};

ShaderCompiler::ShaderCompiler(ShaderFileReader reader)
    : m_reader(std::move(reader))
{
    GlslangProcess::acquire();
}

ShaderCompiler::~ShaderCompiler()
{
    GlslangProcess::release();
}

const ShaderCompiler::CacheEntry* ShaderCompiler::findCached(
    const ShaderCompileRequest& request,
    const std::string& source
) const
{
    for (const auto& entry : m_cache) {
        if (entry->request.path != request.path || entry->request.stage != request.stage
            || entry->request.defines != request.defines || entry->source != source) {
            continue;
        }
        const bool includesSame = std::ranges::all_of(entry->includes, [&](const IncludedFile& file) {
            return readShaderFile(m_reader, file.path) == file.text;
        });
        if (includesSame) {
            return entry.get();
        }
    }

    return nullptr;
}

ShaderCompileResult ShaderCompiler::compile(const ShaderCompileRequest& request)
{
    const std::optional<std::string> source = readShaderFile(m_reader, request.path);
    if (!source) {
        return {.errors = {{.file = request.path, .line = 0, .text = "file not found"}}};
    }
    if (const CacheEntry* cached = findCached(request, *source)) {
        return cached->result;
    }
    ++m_compilations;

    const EShLanguage stage = glslangStage(request.stage);
    glslang::TShader shader(stage);
    const char* text = source->c_str();
    const int length = static_cast<int>(source->size());
    const char* name = request.path.c_str();
    shader.setStringsWithLengthsAndNames(&text, &length, &name, 1);
    const std::string prefix = preamble(request.defines);
    shader.setPreamble(prefix.c_str());
    shader.setEnvInput(glslang::EShSourceGlsl, stage, glslang::EShClientVulkan, kVulkanInputVersion);
    shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_0);
    shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_0);

    const auto messages = static_cast<EShMessages>(EShMsgSpvRules | EShMsgVulkanRules);
    Includer includer(m_reader);
    auto entry = std::make_unique<CacheEntry>();
    entry->request = request;
    entry->source = *source;

    if (!shader.parse(GetDefaultResources(), kGlslVersion, false, messages, includer)) {
        entry->result.errors = parseLog(shader.getInfoLog(), request.path);
    } else {
        glslang::TProgram program;
        program.addShader(&shader);
        if (!program.link(messages)) {
            entry->result.errors = parseLog(program.getInfoLog(), request.path);
        } else {
            glslang::SpvOptions options;
            // Имена переменных и блоков нужны отражению и бэкенду OpenGL.
            options.disableOptimizer = true;
            glslang::GlslangToSpv(*program.getIntermediate(stage), entry->result.spirv, &options);
        }
    }
    entry->includes = includer.included();
    m_cache.push_back(std::move(entry));

    return m_cache.back()->result;
}

} // namespace yqlora
