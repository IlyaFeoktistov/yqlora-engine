#pragma once

#include "yqlora/material/MaterialData.hpp"
#include "yqlora/resource/ResourceId.hpp"

#include <functional>

namespace yqlora::editor {

class EditorSession;

// Правка файла материала в инспекторе. Изменение сразу уходит в кэш ресурсов
// (новая ревизия: все объекты с материалом и превью меняются, пока движется
// ползунок), а файл пишется атомарно, когда правка закончена, и не чаще
// kWritesPerSecond раз в секунду при долгом перетаскивании. Истории правок
// сцены это не касается: файл материала - не сцена.
class MaterialEditing {
public:
    static constexpr float kWritesPerSecond = 2.0f;

    explicit MaterialEditing(EditorSession& session)
        : m_session(session)
    {
    }

    // Материал id, каким его видят объекты; nullptr - не загружен.
    const MaterialData* material(ResourceId id) const;

    // Материал id - свой файл проекта: его можно править. Встроенный и
    // материал модели (isResourceFile) только читаются.
    bool editable(ResourceId id) const;

    // Меняет копию материала id и ставит её в кэш. false, если материал не
    // загружен или не editable.
    bool change(ResourceId id, const std::function<void(MaterialData&)>& edit);

    // Правка закончена (отпустили ползунок, выбрали значение): файл
    // пишется, если менялся. false, если не записался.
    bool finish();

    // Кадр редактора: при долгой правке файл пишется по ходу.
    void update(float deltaSeconds);

    // Есть изменения, ещё не записанные в файл.
    bool pending() const
    {
        return m_pending != 0;
    }

private:
    bool write();

    EditorSession& m_session;
    ResourceId m_pending = 0;
    float m_sinceWrite = 0.0f;
};

} // namespace yqlora::editor
