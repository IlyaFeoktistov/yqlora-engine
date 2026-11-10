#pragma once

#include "yqlora/core/Export.hpp"

#include <memory>

namespace yqlora {

class Input;
class PrefabBases;
class Project;
class ResourceCache;
class TypeRegistry;

// Службы движка, общие для всех сцен процесса. Их даёт сцене её владелец:
// игра, редактор или тест. Глобальной точки доступа нет: компонент берёт
// службы у своей сцены.
class YQLORA_API EngineServices {
public:
    EngineServices(const Input& input, const TypeRegistry& registry);
    // Те же реестр, проект и кэш ресурсов, но свой ввод: так живёт копия
    // сцены для Play. Кэш общий, файлы не читаются второй раз.
    EngineServices(const Input& input, const EngineServices& shared);
    ~EngineServices();

    EngineServices(const EngineServices&) = delete;
    EngineServices& operator=(const EngineServices&) = delete;

    // Пустые службы: ничего не нажато, реестр без типов, проекта нет (из
    // ресурсов только встроенные). Для сцен,
    // которые только читают и пишут файлы. В игре компоненты такой сцены не
    // вызываются: без реестра сцена не знает их фаз.
    static const EngineServices& none();

    const Input& input() const
    {
        return m_input;
    }

    const TypeRegistry& registry() const
    {
        return m_registry;
    }

    // nullptr, пока проект не открыт.
    const Project* project() const
    {
        return m_project;
    }

    // Кэш ресурсов получает тот же проект (и у служб, которые его делят).
    void setProject(Project* project);

    // Загруженные ресурсы: встроенные и файлы открытого проекта. Кэш грузит
    // файлы при обращении, поэтому меняется и у неизменяемых служб.
    ResourceCache& resources() const
    {
        return *m_resources;
    }

    // Развёрнутые деревья префабов, общие для сцен с тем же кэшем ресурсов.
    PrefabBases& prefabBases() const
    {
        return *m_prefabBases;
    }

private:
    const Input& m_input;
    const TypeRegistry& m_registry;
    Project* m_project = nullptr;
    std::shared_ptr<ResourceCache> m_resources;
    std::shared_ptr<PrefabBases> m_prefabBases;
};

} // namespace yqlora
