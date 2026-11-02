#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/rhi/ShaderReflection.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace yqlora {

struct ShaderDefine {
    std::string name{};
    std::string value{};

    bool operator==(const ShaderDefine&) const = default;
};

// Ошибка компиляции с файлом и строкой. line = 0 - без строки (например,
// файл не найден).
struct ShaderMessage {
    std::string file{};
    int line = 0;
    std::string text{};
};

// "file:line: text" - как у компиляторов, чтобы IDE и лог понимали одинаково.
YQLORA_API std::string formatShaderMessage(const ShaderMessage& message);

struct ShaderCompileResult {
    std::vector<std::uint32_t> spirv{};
    std::vector<ShaderMessage> errors{};
    // Пути подключённых файлов в порядке подключения, и при ошибке тоже:
    // изменился любой из них - шейдер компилируется заново.
    std::vector<std::string> includes{};

    bool ok() const
    {
        return errors.empty();
    }
};

// Читает файл шейдера по пути: свой у встроенных шейдеров (вшиты в движок)
// и у шейдеров проекта (диск). nullopt - файла нет.
using ShaderFileReader = std::function<std::optional<std::string>(const std::string& path)>;

struct ShaderCompileRequest {
    std::string path{};
    rhi::ShaderStage stage = rhi::ShaderStage::Vertex;
    std::vector<ShaderDefine> defines{};
};

// GLSL 450 в стиле Vulkan -> SPIR-V через glslang.
//
// #include "file" ищется рядом с подключающим файлом через reader,
// #include <yqlora/...> - в библиотеке шейдеров движка (engine/shaders/include).
// Результаты хранятся в памяти до конца работы: повтор с тем же файлом,
// определениями и неизменными подключениями не компилируется заново.
class YQLORA_API ShaderCompiler {
public:
    explicit ShaderCompiler(ShaderFileReader reader);
    ~ShaderCompiler();

    ShaderCompiler(const ShaderCompiler&) = delete;
    ShaderCompiler& operator=(const ShaderCompiler&) = delete;

    ShaderCompileResult compile(const ShaderCompileRequest& request);

    // Текст файла тем же путём, что видит компиляция: engine: - из
    // библиотеки движка, остальное - через reader. nullopt - файла нет.
    std::optional<std::string> read(const std::string& path) const;

    // Сколько раз дело дошло до glslang: для проверки кэша.
    std::size_t compilations() const
    {
        return m_compilations;
    }

private:
    struct CacheEntry;

    const CacheEntry* findCached(const ShaderCompileRequest& request, const std::string& source) const;

    ShaderFileReader m_reader;
    std::vector<std::unique_ptr<CacheEntry>> m_cache;
    std::size_t m_compilations = 0;
};

} // namespace yqlora
