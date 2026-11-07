#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

struct cgltf_data;
struct cgltf_image;

namespace yqlora {

// Байты картинки glTF: из буфера (GLB, вид буфера), из data URI или из файла
// рядом с моделью file. Файл картинки добавляется в dependencies, и когда его
// нет: появится - модель перечитается. nullopt и причина в error, если
// байтов нет.
std::optional<std::string> readGltfImageBytes(
    const cgltf_image& image,
    const std::filesystem::path& file,
    std::vector<std::filesystem::path>& dependencies,
    std::string& error
);

// Имя картинки для ключа вложенной текстуры: её name, иначе имя файла без
// расширения, иначе пусто.
std::string gltfImageName(const cgltf_image& image);

} // namespace yqlora
