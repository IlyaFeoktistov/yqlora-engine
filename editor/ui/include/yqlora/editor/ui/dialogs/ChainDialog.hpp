#pragma once

#include "yqlora/editor/ChainBuilder.hpp"
#include "yqlora/editor/ui/widgets/WorldPointField.hpp"

namespace yqlora::editor {

class EditorSession;

// Окно "Цепь": концы, шаг и толщина звеньев, вид скреплений и какие концы
// закрепить; кнопка "Создать" ставит цепь одним шагом отмены.
class ChainDialog {
public:
    // Окно откроется в следующем кадре.
    void open()
    {
        m_requested = true;
    }

    void draw(EditorSession& session);

private:
    bool m_requested = false;
    ChainSettings m_settings;
    WorldPointFields m_points;
};

} // namespace yqlora::editor
