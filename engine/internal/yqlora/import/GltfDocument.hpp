#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

struct cgltf_data;

namespace yqlora {

// Файл glTF (.gltf или .glb), разобранный cgltf, с загруженными и
// проверенными буферами. GLB ссылается на свои байты, поэтому они живут
// вместе с данными.
class GltfDocument {
public:
    struct Opened {
        std::unique_ptr<GltfDocument> document;
        std::string error;
        // Внешние файлы буферов, и при ошибке тоже: изменился или появился
        // любой - модель перечитывается.
        std::vector<std::filesystem::path> dependencies;
    };

    static Opened open(const std::filesystem::path& file);

    ~GltfDocument();

    GltfDocument(const GltfDocument&) = delete;
    GltfDocument& operator=(const GltfDocument&) = delete;

    const cgltf_data& data() const
    {
        return *m_data;
    }

private:
    GltfDocument() = default;

    std::vector<char> m_bytes;
    cgltf_data* m_data = nullptr;
};

} // namespace yqlora
