#pragma once

#include <cstdint>

namespace yqlora::editor {

// Выбор строки по отпусканию левой кнопки: нажали и отпустили на той же
// строке, не потянув. Так перетаскивание строки (файл на поле, узел в поле
// скрипта) не меняет выбор, а с ним и инспектор.
class ReleaseClick {
public:
    // Сразу после строки; key - её постоянный ключ, не 0. true - строку
    // выбрали. Нажатие на стрелку раскрытия выбором не считается.
    bool released(std::uint64_t key);

private:
    std::uint64_t m_pressed = 0;
};

} // namespace yqlora::editor
