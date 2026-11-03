#include "yqlora/platform/Files.hpp"

#include <fstream>
#include <iterator>
#include <system_error>

namespace yqlora::platform {

namespace {

constexpr std::string_view kTemporarySuffix = ".tmp";

} // namespace

std::optional<std::string> readFile(const std::filesystem::path& file)
{
    std::ifstream stream(file, std::ios::binary);
    if (!stream) {
        return std::nullopt;
    }
    std::string bytes{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    if (stream.bad()) {
        return std::nullopt;
    }

    return bytes;
}

bool writeFileAtomically(const std::filesystem::path& file, std::string_view bytes)
{
    namespace fs = std::filesystem;
    std::error_code ec;
    if (file.has_parent_path()) {
        fs::create_directories(file.parent_path(), ec);
    }
    fs::path temporary = file;
    temporary += kTemporarySuffix;
    bool written = false;
    {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        out.flush();
        written = static_cast<bool>(out);
    }
    if (written) {
        fs::rename(temporary, file, ec);
        written = !ec;
    }
    if (!written) {
        fs::remove(temporary, ec);
    }

    return written;
}

} // namespace yqlora::platform
