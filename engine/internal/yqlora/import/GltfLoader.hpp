#pragma once

#include "yqlora/resource/ResourceLoader.hpp"

namespace yqlora {

// glTF 2.0 (.gltf с внешними или встроенными буферами, .glb). Ресурс файла -
// ModelData (дерево узлов), вложенные - MeshData по мешу файла (примитив -
// подмеш со своим материалом файла), материалы и их текстуры (см.
// GltfMaterials). Файл, который требует неподдерживаемых расширений
// (extensionsRequired), не читается. Данные читаются через аксессоры cgltf,
// поэтому годятся любые раскладки буферов. Только треугольники: примитив
// другого режима пропускается с сообщением в логе.
//
// Настройки импорта: "scale" (число, по умолчанию 1) умножает положения
// вершин и узлов.
class YQLORA_API GltfLoader final : public ResourceLoader {
public:
    static constexpr const char* kScaleSetting = "scale";
    static constexpr float kDefaultScale = 1.0f;

    std::span<const std::string_view> extensions() const override;
    std::string_view kind() const override;
    std::span<const std::string_view> nestedKinds() const override;
    LoadResult load(const LoadRequest& request) const override;
};

} // namespace yqlora
