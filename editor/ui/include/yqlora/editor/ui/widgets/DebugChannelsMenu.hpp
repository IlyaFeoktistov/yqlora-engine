#pragma once

#include "yqlora/debug/DebugChannels.hpp"

namespace yqlora {
class Scene;
} // namespace yqlora

namespace yqlora::editor {

class EditorSettings;

// Кнопка "Гизмо" окна и её меню: каналы отладочной отрисовки (DebugDraw),
// которые окно показывает. В списке - каналы систем сцены и уже включённые;
// выбор сразу сохраняется в настройках. Рисуется там, где стоит курсор
// интерфейса. Возвращает true, если курсор над кнопкой.
bool debugChannelsMenu(const char* id, const Scene& scene, DebugChannels& shown, EditorSettings& settings);

} // namespace yqlora::editor
