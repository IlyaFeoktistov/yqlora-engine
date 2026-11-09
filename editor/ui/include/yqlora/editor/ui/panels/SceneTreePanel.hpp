#pragma once

#include "yqlora/resource/ResourceId.hpp"

#include <optional>

#include "yqlora/editor/ui/core/EditorPanel.hpp"
#include "yqlora/editor/ui/dialogs/AddNodeDialog.hpp"
#include "yqlora/editor/ui/widgets/ReleaseClick.hpp"

#include "yqlora/scene/Node.hpp"

namespace yqlora::editor {

class Selection;

// Дерево узлов сцены. Выбор здесь и в окне сцены общий: узел, выбранный
// кликом в 3D, подсвечивается в дереве, а его родители раскрываются.
// Отсюда же узлы добавляются и удаляются.
class SceneTreePanel final : public EditorPanel {
public:
    const char* id() const override
    {
        return "Scene";
    }

    const char* titleKey() const override
    {
        return "editor.window.scene";
    }

    void draw(EditorContext& context) override;

private:
    void drawNode(const Node& node, Selection& selection);
    // Перенос, отпущенный в этом кадре: сразу по запомненному выбору или
    // через окно с вопросом.
    void handleMove(EditorContext& context);
    void drawKeepWorldDialog(EditorContext& context);

    AddNodeDialog m_addNode;
    // Что было выбрано в прошлом кадре: если выбор сменился не здесь,
    // дерево раскрывается и прокручивается к выбранному узлу.
    NodeId m_lastSelected = 0;
    ReleaseClick m_release;
    bool m_reveal = false;
    // Узел, который просили показать (Pings): путь до него раскрывается,
    // строка прокручивается в вид и подсвечивается.
    NodeId m_pinged = 0;
    double m_pingedAt = 0.0;
    bool m_scrollToPinged = false;
    // Удалять узлы можно только после обхода дерева.
    bool m_deleteRequested = false;
    // Ресурс, отпущенный над узлом (или над пустым местом - 0), тоже ставится
    // в сцену после обхода.
    struct Drop {
        ResourceId resource = 0;
        NodeId parent = 0;
    };
    std::optional<Drop> m_drop;
    // Узел, отпущенный над другим узлом: перенос к нему. Alt при отпускании
    // один раз делает наоборот запомненному выбору.
    struct Move {
        NodeId node = 0;
        NodeId parent = 0;
        bool invert = false;
    };
    std::optional<Move> m_move;
    // Перенос ждёт ответа в окне "Сохранить положение в мире?".
    std::optional<Move> m_asking;
    bool m_askRequested = false;
    bool m_rememberChoice = false;
};

} // namespace yqlora::editor
