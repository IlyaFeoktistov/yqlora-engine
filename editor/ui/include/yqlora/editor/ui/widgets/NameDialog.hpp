#pragma once

#include <functional>
#include <string>
#include <string_view>

namespace yqlora::editor {

struct EditorColors;

// Окно с полем имени и кнопкой подтверждения: через него создаётся,
// копируется и переименовывается всё в редакторе. Действие выполняется
// только по кнопке (или Enter), отмена ничего не оставляет.
class NameDialog {
public:
    struct Text {
        // Заголовок; постоянный ID после "###", если подпись меняется.
        std::string title;
        // Подпись над полем; пусто - без неё.
        std::string label;
        std::string confirm;
        // Ошибка, когда имя не годится (valid).
        std::string invalid;
        // Ошибка, когда действие не удалось (имя занято).
        std::string failed;
    };

    // Окно откроется при следующем draw с предложенным именем name.
    void open(std::string name);

    // Рисует окно, если оно открыто. valid - имя годится; confirm выполняет
    // действие и возвращает true, если оно удалось: тогда окно закрывается.
    void draw(
        const Text& text,
        const std::function<bool(std::string_view)>& valid,
        const std::function<bool(const std::string&)>& confirm,
        const EditorColors& colors
    );

private:
    std::string m_name;
    bool m_requested = false;
    bool m_failed = false;
};

} // namespace yqlora::editor
