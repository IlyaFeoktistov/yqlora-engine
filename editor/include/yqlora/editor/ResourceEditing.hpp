#pragma once

#include "yqlora/resource/ResourceCache.hpp"
#include "yqlora/resource/ResourceId.hpp"

#include <functional>

namespace yqlora::editor {

class EditorSession;

// Правка файла ресурса в инспекторе (материал, физический материал).
// Изменение сразу уходит в кэш ресурсов (новая ревизия: всё, что берёт
// ресурс, и превью меняются, пока движется ползунок), а файл пишется
// атомарно, когда правка закончена, и не чаще kWritesPerSecond раз в
// секунду при долгом перетаскивании. Какие виды правятся и как пишется их
// файл - таблица в ResourceEditing.cpp. Истории правок сцены это не
// касается: файл ресурса - не сцена.
class ResourceEditing {
public:
    static constexpr float kWritesPerSecond = 2.0f;

    explicit ResourceEditing(EditorSession& session)
        : m_session(session)
    {
    }

    // Ресурс id вида T, каким его видят объекты; nullptr - не загружен или
    // другого вида.
    template <typename T>
    const T* find(ResourceId id) const
    {
        return cache().find<T>(id);
    }

    // Ресурс id - свой файл проекта вида, который умеет писаться: его можно
    // править. Встроенный и вложенный (материал модели, isResourceFile)
    // только читаются.
    bool editable(ResourceId id) const;

    // Меняет копию ресурса id и ставит её в кэш. false, если ресурс не
    // загружен, другого вида или не editable.
    template <typename T>
    bool change(ResourceId id, const std::function<void(T&)>& edit)
    {
        const T* current = find<T>(id);
        if (!current || !editable(id)) {
            return false;
        }
        T changed = *current;
        edit(changed);
        beforeChange(id);
        if (!cache().replace(id, std::move(changed))) {
            return false;
        }
        afterChange(id);

        return true;
    }

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
    ResourceCache& cache() const;
    // Другой ресурс: правка прежнего дописывается в его файл.
    void beforeChange(ResourceId id);
    void afterChange(ResourceId id);
    bool write();

    EditorSession& m_session;
    ResourceId m_pending = 0;
    float m_sinceWrite = 0.0f;
};

} // namespace yqlora::editor
