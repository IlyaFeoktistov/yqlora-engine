#pragma once

#include "yqlora/math/Math.hpp"

#include <string>

namespace yqlora {

class Scene;

} // namespace yqlora

namespace yqlora::editor {

// Куда смотрит камера новой сцены: на объект, стоящий на сетке в начале координат.
inline constexpr Vec3 kNewSceneFocus{0.0f, 0.5f, 0.0f};

// Каждая новая сцена начинается с корня, камеры, солнца (направленный свет
// с флажком sun) и окружения (небо, окружающий свет, атмосфера): новый
// объект сразу и виден, и освещён.
void buildNewScene(Scene& scene, std::string rootName);

} // namespace yqlora::editor
