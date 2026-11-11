#include "yqlora/editor/ScriptToolchain.hpp"

#include "yqlora/core/JsonFile.hpp"
#include "yqlora/core/Utf8Path.hpp"
#include "yqlora/platform/Platform.hpp"

#include <nlohmann/json.hpp>

namespace yqlora::editor {

namespace {

// Папка и файл, которые пишет cmake/Sdk.cmake: имена должны совпадать с ним.
constexpr const char* kSdkFolder = "sdk";
constexpr const char* kToolchainFile = "toolchain.json";

constexpr const char* kCmakeKey = "cmake";
constexpr const char* kGeneratorKey = "generator";
constexpr const char* kMakeProgramKey = "makeProgram";
constexpr const char* kArchitectureKey = "architecture";
constexpr const char* kCKey = "c";
constexpr const char* kCxxKey = "cxx";
constexpr const char* kPathKey = "path";
constexpr const char* kIdKey = "id";
constexpr const char* kVersionKey = "version";
constexpr const char* kVcvarsallKey = "vcvarsall";

std::string stringOf(const nlohmann::json& json, const char* key)
{
    const auto it = json.find(key);

    return it != json.end() && it->is_string() ? it->get<std::string>() : std::string();
}

void overrideIfSet(std::filesystem::path& target, const std::filesystem::path& value)
{
    if (!value.empty()) {
        target = value;
    }
}

} // namespace

std::string ScriptToolchain::fingerprint() const
{
    // Разделитель, которого нет в путях и версиях.
    constexpr char kSeparator = '\n';
    std::string text;
    for (const std::string& part :
         {toUtf8(cmake),
          generator,
          toUtf8(makeProgram),
          toUtf8(cCompiler),
          toUtf8(cxxCompiler),
          compilerId,
          compilerVersion,
          architecture,
          toUtf8(vcvarsall)}) {
        text += part;
        text += kSeparator;
    }

    return text;
}

std::filesystem::path sdkDirectory()
{
    return platform::executableDir() / kSdkFolder;
}

std::optional<ScriptToolchain> loadScriptToolchain(const std::filesystem::path& sdk)
{
    const auto json = readJsonFile(sdk / kToolchainFile);
    if (!json || !json->is_object() || !json->contains(kCxxKey) || !(*json)[kCxxKey].is_object()) {
        return std::nullopt;
    }
    const nlohmann::json& c = json->value(kCKey, nlohmann::json::object());
    const nlohmann::json& cxx = (*json)[kCxxKey];

    ScriptToolchain toolchain;
    toolchain.cmake = fromUtf8(stringOf(*json, kCmakeKey));
    toolchain.generator = stringOf(*json, kGeneratorKey);
    toolchain.makeProgram = fromUtf8(stringOf(*json, kMakeProgramKey));
    toolchain.architecture = stringOf(*json, kArchitectureKey);
    toolchain.cCompiler = fromUtf8(stringOf(c, kPathKey));
    toolchain.cxxCompiler = fromUtf8(stringOf(cxx, kPathKey));
    toolchain.compilerId = stringOf(cxx, kIdKey);
    toolchain.compilerVersion = stringOf(cxx, kVersionKey);
    toolchain.vcvarsall = fromUtf8(stringOf(*json, kVcvarsallKey));
    if (toolchain.cmake.empty() || toolchain.generator.empty() || toolchain.cxxCompiler.empty()) {
        return std::nullopt;
    }

    return toolchain;
}

void applyOverrides(ScriptToolchain& toolchain, const ToolchainOverrides& overrides)
{
    for (const ToolchainOverrideField& field : kToolchainOverrideFields) {
        overrideIfSet(toolchain.*field.recorded, overrides.*field.override);
    }
}

} // namespace yqlora::editor
