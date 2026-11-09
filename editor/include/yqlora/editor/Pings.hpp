#pragma once

#include "yqlora/resource/ResourceId.hpp"
#include "yqlora/scene/NodeId.hpp"

#include <optional>

namespace yqlora::editor {

// Просьбы показать цель ссылки, не меняя выбора: щелчок по заполненному полю
// узла подсвечивает узел в дереве сцены, поля ресурса - файл в "Файлах".
// Поле просит, панель забирает свою просьбу, раскрывает путь к цели,
// прокручивает к ней и подсвечивает.
class Pings {
public:
    void node(NodeId id)
    {
        m_node = id;
    }

    void resource(ResourceId id)
    {
        m_resource = id;
    }

    // Просьба забирается один раз; nullopt - её нет.
    std::optional<NodeId> takeNode()
    {
        return take(m_node);
    }

    std::optional<ResourceId> takeResource()
    {
        return take(m_resource);
    }

private:
    template <typename Id>
    static std::optional<Id> take(std::optional<Id>& request)
    {
        std::optional<Id> taken = request;
        request.reset();

        return taken;
    }

    std::optional<NodeId> m_node;
    std::optional<ResourceId> m_resource;
};

} // namespace yqlora::editor
