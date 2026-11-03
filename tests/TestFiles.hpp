#pragma once

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string_view>

namespace yqlora::test {

// Шаг времени записи при перезаписи: его замечают и файловые системы с
// грубыми отметками времени.
inline constexpr std::chrono::seconds kWriteTimeStep{2};

// Пустая папка yqlora_tests/name во временной папке системы; прежнее
// содержимое удаляется.
inline std::filesystem::path freshDirectory(const std::filesystem::path& name)
{
    const auto directory = std::filesystem::temp_directory_path() / "yqlora_tests" / name;
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);

    return directory;
}

// Файл с содержимым content; папки создаются.
inline void writeFile(const std::filesystem::path& file, std::string_view content)
{
    std::filesystem::create_directories(file.parent_path());
    std::ofstream(file, std::ios::binary | std::ios::trunc) << content;
}

// Время записи файла после before: так изменение заметит сравнение по времени.
inline void markRewritten(const std::filesystem::path& file, std::filesystem::file_time_type before)
{
    std::filesystem::last_write_time(file, before + kWriteTimeStep);
}

// Перезапись, которую заметит сравнение по времени записи.
inline void rewriteFile(const std::filesystem::path& file, std::string_view content)
{
    const auto before = std::filesystem::last_write_time(file);
    writeFile(file, content);
    markRewritten(file, before);
}

} // namespace yqlora::test
