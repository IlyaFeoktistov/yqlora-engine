#pragma once

#include "yqlora/math/Math.hpp"

namespace yqlora::editor {

class EditorSession;

// Точка мира: поле и кнопка, которая берёт положение выбранного узла.
void worldPointField(const char* labelKey, Vec3& point, EditorSession& session);

} // namespace yqlora::editor
