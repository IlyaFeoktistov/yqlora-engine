#pragma once

#include "yqlora/resource/ResourceLoader.hpp"

#include <filesystem>
#include <string>
#include <vector>

struct cgltf_data;

namespace yqlora {

// Текстуры и материалы файла glTF вложенными ресурсами (см. "glTF materials"
// в docs/architecture/materials.md). Материал - шейдер Standard; его
// текстуры - ссылки по ключу (NestedLink) на текстуры того же файла.
struct GltfMaterials {
    // Текстуры в порядке картинок файла, собранные ormMap, затем материалы.
    std::vector<LoadedResource> resources;
    // Ключ материала по индексу материала в файле.
    std::vector<std::string> materialKeys;
    // Файлы картинок рядом с моделью.
    std::vector<std::filesystem::path> dependencies;
};

// file - сам файл модели: от него ищутся картинки и пишутся сообщения.
// Картинка, которая не прочиталась, - предупреждение в логе, а материал
// берёт текстуру по умолчанию.
GltfMaterials readGltfMaterials(const cgltf_data& data, const std::filesystem::path& file, SubresourceKeys& keys);

} // namespace yqlora
