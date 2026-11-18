#pragma once

#include "yqlora/editor/BodyChainBuilder.hpp"
#include "yqlora/editor/ui/widgets/WorldPointField.hpp"

namespace yqlora::editor {

class EditorSession;

// Окно "Цепь тел": имя, место, число звеньев, длина, толщина и масса;
// кнопка "Создать" ставит цепь одним шагом отмены.
class BodyChainDialog {
public:
    // Окно откроется в следующем кадре.
    void open()
    {
        m_requested = true;
    }

    void draw(EditorSession& session);

private:
    bool m_requested = false;
    BodyChainSettings m_settings;
    WorldPointFields m_points;
};

} // namespace yqlora::editor
