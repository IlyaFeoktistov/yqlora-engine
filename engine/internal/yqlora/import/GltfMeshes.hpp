#pragma once

#include "yqlora/render/MeshData.hpp"

#include <cstddef>
#include <string>
#include <string_view>

struct cgltf_data;

namespace yqlora {

// Меш index файла glTF: примитив - подмеш со своим материалом файла, только
// треугольники (примитив другого режима пропускается с сообщением в логе),
// положения умножены на scale. Нормалей нет - считаются сглаженные;
// касательных нет, а у материала примитива карта нормалей - MikkTSpace. Текст
// ошибки, если данные не годятся; fileName - для сообщений.
std::string readGltfMesh(
    const cgltf_data& data,
    std::size_t index,
    float scale,
    std::string_view fileName,
    MeshData& mesh
);

} // namespace yqlora
