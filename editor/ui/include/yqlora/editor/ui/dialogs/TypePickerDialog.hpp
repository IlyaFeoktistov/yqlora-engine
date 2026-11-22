#pragma once

#include "yqlora/editor/TypeCatalog.hpp"

#include "yqlora/scene/NodeId.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace yqlora::editor {

struct EditorContext;

// Окно выбора типа для "Добавить компонент" и "Добавить узел": полки слева,
// типы полки справа (значок, имя на языке человека, строка описания),
// подробности выбранного внизу. Неподходящий узлу тип виден серым с
// причиной. Ввод сразу идёт в поиск; стрелки двигают выбор, Enter
// добавляет, Escape закрывает. Что предлагать, решает TypeCatalog.
class TypePickerDialog {
public:
    // Окно откроется в следующем кадре.
    void openComponents(NodeId node);
    // parent - куда добавить узел, 0 - в корень.
    void openNodes(NodeId parent);

    void draw(EditorContext& context);

private:
    enum class Mode : std::uint8_t {
        Components,
        Nodes,
    };

    void open(Mode mode, NodeId target);
    // Каталог по реестру и узлу; false, если узла уже нет.
    bool rebuild(EditorContext& context);
    void keepShelfInRange();
    void drawShelves();
    // true, если тип выбрали двойным щелчком.
    bool drawList(EditorContext& context, const std::vector<const TypeEntry*>& entries);
    void drawDetails(EditorContext& context, const TypeEntry* entry);
    void moveSelection(const std::vector<const TypeEntry*>& entries, int step);
    void add(EditorContext& context, const TypeEntry& entry);

    bool m_requested = false;
    Mode m_mode = Mode::Components;
    NodeId m_target = 0;
    std::optional<TypeCatalog> m_catalog;
    // Ревизия реестра, по которой построен каталог: модуль игры перезагрузили - строим заново.
    std::uint64_t m_revision = 0;
    std::size_t m_shelf = 0;
    std::string m_search;
    std::string m_selected;
    // Выбор сменили клавишами: список прокручивается к нему.
    bool m_scrollToSelected = false;
};

} // namespace yqlora::editor
