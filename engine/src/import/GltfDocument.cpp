#include "yqlora/import/GltfDocument.hpp"

#include "yqlora/core/Utf8Path.hpp"
#include "yqlora/platform/Files.hpp"

#include <cgltf.h>

#include <cstdlib>
#include <cstring>
#include <string_view>

namespace yqlora {

namespace {

namespace fs = std::filesystem;

// Буфер, вписанный в сам файл (base64), а не лежащий рядом.
constexpr std::string_view kDataUriPrefix = "data:";

// cgltf открывает внешние буферы сам через fopen, который на Windows не
// понимает путей не из ASCII. Путь он даёт в UTF-8, а читаем мы.
cgltf_result readFile(
    const cgltf_memory_options* /*memory*/,
    const cgltf_file_options* /*file*/,
    const char* path,
    cgltf_size* size,
    void** data
)
{
    const std::string bytes = platform::readFile(fromUtf8(path)).value_or(std::string());
    if (bytes.empty()) {
        return cgltf_result_file_not_found;
    }
    void* copy = std::malloc(bytes.size());
    if (!copy) {
        return cgltf_result_out_of_memory;
    }
    std::memcpy(copy, bytes.data(), bytes.size());
    *size = bytes.size();
    *data = copy;

    return cgltf_result_success;
}

void releaseFile(const cgltf_memory_options* /*memory*/, const cgltf_file_options* /*file*/, void* data)
{
    std::free(data);
}

const char* resultText(cgltf_result result)
{
    switch (result) {
    case cgltf_result_success: return "success";
    case cgltf_result_data_too_short: return "the data is too short";
    case cgltf_result_unknown_format: return "not a glTF file";
    case cgltf_result_invalid_json: return "invalid JSON";
    case cgltf_result_invalid_gltf: return "invalid glTF";
    case cgltf_result_invalid_options: return "invalid options";
    case cgltf_result_file_not_found: return "a buffer file is not found";
    case cgltf_result_io_error: return "a read error";
    case cgltf_result_out_of_memory: return "out of memory";
    case cgltf_result_legacy_gltf: return "glTF 1.0 is not supported";
    case cgltf_result_max_enum: break;
    }

    return "unknown error";
}

// Файлы буферов рядом с file по их URI: и тех, что ещё не загружены.
std::vector<fs::path> bufferFiles(const cgltf_data& data, const fs::path& file)
{
    std::vector<fs::path> files;
    for (cgltf_size i = 0; i < data.buffers_count; ++i) {
        const char* uri = data.buffers[i].uri;
        if (!uri || std::string_view(uri).starts_with(kDataUriPrefix)) {
            continue;
        }
        std::string decoded(uri);
        decoded.resize(cgltf_decode_uri(decoded.data()));
        files.push_back(file.parent_path() / fromUtf8(decoded));
    }

    return files;
}

} // namespace

GltfDocument::Opened GltfDocument::open(const fs::path& file)
{
    Opened opened;
    std::unique_ptr<GltfDocument> document(new GltfDocument());
    document->m_bytes = platform::readFile(file).value_or(std::string());
    if (document->m_bytes.empty()) {
        opened.error = "the file is empty or not readable";

        return opened;
    }

    cgltf_options options{};
    options.file.read = &readFile;
    options.file.release = &releaseFile;
    cgltf_result result = cgltf_parse(&options, document->m_bytes.data(), document->m_bytes.size(), &document->m_data);
    if (result == cgltf_result_success) {
        opened.dependencies = bufferFiles(*document->m_data, file);
        result = cgltf_load_buffers(&options, document->m_data, toUtf8(file).c_str());
    }
    if (result == cgltf_result_success) {
        result = cgltf_validate(document->m_data);
    }
    if (result != cgltf_result_success) {
        opened.error = resultText(result);

        return opened;
    }
    opened.document = std::move(document);

    return opened;
}

GltfDocument::~GltfDocument()
{
    cgltf_free(m_data);
}

} // namespace yqlora
