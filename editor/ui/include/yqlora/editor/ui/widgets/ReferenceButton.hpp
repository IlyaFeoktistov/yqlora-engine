#pragma once

#include <span>
#include <string>

namespace yqlora::editor {

// Что нажали у поля ссылки.
enum class ReferenceClick {
    None,
    // Само поле: у заполненного - показать цель, у пустого - выбрать.
    Field,
    // Кнопка выбора справа: окно выбора.
    Pick,
};

// Поле ссылки (на ресурс, узел, компонент): кнопка во всю ширину с подписью
// и кнопка выбора справа, всё одной группой - её делают целью броска.
ReferenceClick referenceButton(const std::string& label, const std::string& tooltip);

// Список выбора с поиском в открытом окне: "Нет" и labels, отфильтрованные по
// search. Возвращает выбранный номер, kNone для "Нет", kNothing - ничего.
struct ChoiceList {
    static constexpr int kNothing = -2;
    static constexpr int kNone = -1;
};
int drawChoiceList(std::span<const std::string> labels, int current, std::string& search);

} // namespace yqlora::editor
