#pragma once

namespace yqlora::editor {

// Кнопка с иконкой на панелях окон, подсвеченная, когда on (инструмент
// выбран, режим включён). Возвращает true по щелчку.
bool iconButton(const char* icon, bool on);

// Переключатель: щелчок меняет value. Возвращает true, если value изменилось.
bool iconToggle(const char* icon, bool& value);

// Ширина кнопки с иконкой: чтобы ставить кнопки от правого края окна.
float iconButtonWidth(const char* icon);

} // namespace yqlora::editor
