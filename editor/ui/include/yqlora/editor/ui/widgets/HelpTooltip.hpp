#pragma once

#include <string>

namespace yqlora::editor {

// Подсказка последнего элемента с описанием: длинный текст переносится по
// словам на удобную для чтения ширину. Пустой текст - без подсказки.
void helpTooltip(const std::string& text);

} // namespace yqlora::editor
