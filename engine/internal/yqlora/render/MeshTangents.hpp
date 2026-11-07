#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/render/MeshData.hpp"

#include <cstdint>
#include <vector>

namespace yqlora {

// Касательные для карты нормалей по положениям, нормалям и uv0 (MikkTSpace:
// так их считают glTF и программы, в которых пекут карты, поэтому карта
// ложится без швов). Касательная считается для каждого угла треугольника;
// вершина, у которой углы с разной касательной (шов развёртки, зеркальная
// половина), делится: копия добавляется в конец vertices, и indices этих
// углов переставляются на неё. Индексы - тройками, все в пределах vertices.
// false, если треугольников нет или MikkTSpace не справилась (не хватило
// памяти): касательные остаются как были. У вырожденной развёртки
// касательные есть, но любые: такой меш карту нормалей всё равно не носит.
YQLORA_API bool computeTangents(std::vector<Vertex>& vertices, std::vector<std::uint32_t>& indices);

} // namespace yqlora
