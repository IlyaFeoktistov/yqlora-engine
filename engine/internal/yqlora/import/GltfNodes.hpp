#pragma once

#include "yqlora/import/ModelData.hpp"

#include <cstddef>
#include <vector>

struct cgltf_data;

namespace yqlora {

// Узел index файла glTF с локальным трансформом; положение умножено на scale.
ModelNode readGltfNode(const cgltf_data& data, std::size_t index, float scale);

// Узлы верхнего уровня: сцены файла по умолчанию (или первой), а без сцен -
// все узлы без родителя.
std::vector<std::size_t> gltfRoots(const cgltf_data& data);

} // namespace yqlora
