#pragma once

#include "yqlora/math/Math.hpp"
#include "yqlora/resource/ResourceKinds.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace yqlora {

// Узел модели из файла с локальным трансформом относительно родителя.
struct ModelNode {
    std::string name;
    Vec3 position{0.0f};
    Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    Vec3 scale{1.0f};
    // Индекс в ModelData::meshes; nullopt - узел без меша.
    std::optional<std::size_t> mesh;
    // Индексы в ModelData::nodes.
    std::vector<std::size_t> children;
};

// Дерево узлов модели, ресурс самого файла модели: по нему модель ставится в
// сцену. Меши - вложенные ресурсы файла; здесь только их ключи, ID даёт .meta
// (ResourceDatabase::subresources). Один меш у нескольких узлов - один ключ.
struct ModelData {
    static constexpr std::string_view kResourceKind = kinds::kModel;

    std::vector<ModelNode> nodes;
    // Узлы верхнего уровня, индексы в nodes.
    std::vector<std::size_t> roots;
    // Ключи вложенных мешей по индексу меша в файле.
    std::vector<std::string> meshes;
};

} // namespace yqlora
