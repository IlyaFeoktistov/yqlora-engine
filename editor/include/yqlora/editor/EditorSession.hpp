#pragma once

#include "yqlora/editor/Command.hpp"
#include "yqlora/editor/EditHistory.hpp"
#include "yqlora/editor/GameLibrary.hpp"
#include "yqlora/editor/MaterialEditing.hpp"
#include "yqlora/editor/Pings.hpp"
#include "yqlora/editor/PlayMode.hpp"
#include "yqlora/editor/PrefabEditing.hpp"
#include "yqlora/editor/PreviewLighting.hpp"
#include "yqlora/editor/SceneDocument.hpp"
#include "yqlora/editor/SceneEditing.hpp"
#include "yqlora/editor/Selection.hpp"
#include "yqlora/editor/SettingsEditing.hpp"
#include "yqlora/project/Project.hpp"
#include "yqlora/reflection/Property.hpp"
#include "yqlora/scene/Transform3D.hpp"

#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>

namespace yqlora {

class Component;
class EngineServices;
class GameModule;
class Node;
class Node3D;
class ResourceCache;
class Scene;
class SceneSerializer;
class TypeInfo;
class TypeRegistry;

} // namespace yqlora

namespace yqlora::editor {

// Открытый в редакторе проект: его сцена, настройки, выделение и игра в
// редакторе. Без интерфейса, панели только вызывают эти методы. Сцена и
// настройки меняются только командами, поэтому любую правку можно отменить.
// Сама сессия согласует части между собой (выделение при Play и Stop,
// история при открытии сцены) и ведёт общую историю правок; файл сцены
// ведёт SceneDocument, игру - PlayMode.
class EditorSession {
public:
    // Открытый проект сессия ставит в services: его видят сцены редактора.
    // gameModule - для библиотеки игры открытого проекта, на реестре serializer.
    EditorSession(EngineServices& services, const SceneSerializer& serializer, GameModule& gameModule);

    const std::optional<Project>& project() const
    {
        return m_project;
    }

    std::optional<Project>& project()
    {
        return m_project;
    }

    // Сцена, с которой сейчас работают: во время игры - её копия для игры,
    // иначе редактируемая.
    Scene& scene()
    {
        return m_play.playing() ? *m_play.scene() : m_document.scene();
    }

    const Scene& scene() const
    {
        return m_play.playing() ? *m_play.scene() : m_document.scene();
    }

    const SceneDocument& document() const
    {
        return m_document;
    }

    const PlayMode& playMode() const
    {
        return m_play;
    }

    Selection& selection()
    {
        return m_selection;
    }

    const Selection& selection() const
    {
        return m_selection;
    }

    // Загруженные ресурсы: общие у редактируемой сцены и копии Play.
    ResourceCache& resources() const;

    // Правка текущей сцены командами с отменой.
    SceneEditing& editing()
    {
        return m_editing;
    }

    // Экземпляры префабов: вернуть, применить, распаковать, перестроить.
    PrefabEditing& prefabEditing()
    {
        return m_prefabEditing;
    }

    // Правка настроек проекта командами с отменой.
    SettingsEditing& settingsEditing()
    {
        return m_settingsEditing;
    }

    // Службы движка, в которых живёт сессия: ресурсы, ввод, реестр типов.
    EngineServices& services() const
    {
        return m_services;
    }

    // Правка файлов материалов в инспекторе, см. MaterialEditing.
    MaterialEditing& materialEditing()
    {
        return m_materialEditing;
    }

    // Просьбы показать цель ссылки в дереве и в "Файлах", см. Pings.
    Pings& pings()
    {
        return m_pings;
    }

    // Правка закончилась (отпустили мышь, ушли из поля ввода): следующая
    // будет отдельным шагом истории.
    void finishEdit()
    {
        m_history.finishEdit();
    }

    // Во время игры истории нет: по Stop сцена всё равно вернётся как была.
    bool canUndo() const;
    bool canRedo() const;
    bool undo();
    bool redo();

    // Сцена отличается от своего файла (см. SceneDocument::modified). Для
    // заголовка окна хватает значения, которое сессия обновляет после правок
    // и раз в kModifiedCheckSeconds; перед вопросом о несохранённом стоит
    // вызвать refreshModified.
    bool sceneModified() const
    {
        return m_document.modified();
    }

    // Есть несохранённое: сцена или разделы настроек проекта.
    bool modified() const;

    // Сравнить сцену с файлом сейчас.
    void refreshModified()
    {
        m_document.refreshModified();
    }

    // Типы, которые знает сессия: те же, что у её сериализатора.
    const TypeRegistry& registry() const;

    const SceneSerializer& serializer() const
    {
        return m_serializer;
    }

    // Открывает проект: сначала собранную библиотеку игры gameLibrary (пусто -
    // её ещё нет), чтобы скрипты в сцене были настоящими, потом стартовую
    // сцену. Загружается копия библиотеки, см. GameLibrary. Если сцена не
    // читается, проект всё равно открывается с пустой сценой, а метод
    // возвращает false.
    bool open(Project project, const std::filesystem::path& gameLibrary = {});
    // Библиотека игры выгружается последней: к этому моменту ни в сценах, ни в
    // истории правок нет объектов её типов.
    void close();

    // Горячая перезагрузка: останавливает игру, запоминает сцену в памяти,
    // пока поля скриптов может прочитать старый код, очищает историю правок
    // (её команды держат объекты старого кода), выгружает модуль, загружает
    // копию gameLibrary и читает сцену обратно, с тем же выделением. Файл
    // сцены не трогается. Если новая библиотека не загрузилась, скрипты сцены
    // остаются заглушками со всеми данными до следующей удачной загрузки;
    // тогда false.
    bool reloadGameModule(const std::filesystem::path& gameLibrary);

    // Собранная библиотека, копия которой загружена; пусто - модуль не загружен.
    const std::filesystem::path& gameLibrary() const;

    // Файлы проекта могли поменять мимо редактора (вернулись из другой
    // программы): обходит папку проекта и перечитывает изменённые ресурсы;
    // экземпляры изменившихся префабов перестраиваются.
    void refreshResources();

    // Путь к открытой сцене от папки проекта. Пусто, если сцены нет.
    std::filesystem::path scenePath() const;

    // Открыт префаб, а не сцена.
    bool editingPrefab() const;

    // Свет, которым окно сцены освещает открытый префаб (PreviewLighting);
    // nullptr, если открыт не префаб.
    const Scene* prefabLighting();

    // Открывает другую сцену проекта. Если файл не читается, остаётся прежняя.
    bool openScene(const std::filesystem::path& path);

    // Сохраняет проект: сцену в её файл и изменённые разделы настроек. Во
    // время игры нельзя: в сцене состояние игры.
    bool save();

    bool playing() const
    {
        return m_play.playing();
    }

    // Запускает игру в копии редактируемой сцены, см. PlayMode.
    void play();
    void setPaused(bool paused)
    {
        m_play.setPaused(paused);
    }

    // Выбрасывает копию для игры и возвращается к редактируемой сцене.
    // Выделение остаётся на узлах, которые есть и в ней.
    void stop();

    // Кадр игры: обновляет копию сцены, пока игра идёт и не на паузе.
    // input - ввод приложения, игра видит его, только пока окно игры в фокусе.
    void update(float deltaSeconds, const Input& input)
    {
        updateModified(deltaSeconds);
        m_materialEditing.update(deltaSeconds);
        m_play.update(deltaSeconds, input);
        // Сцену документа правят между кадрами: мировые матрицы одним проходом.
        m_document.scene().updateTransforms();
    }

    void setGameInputFocused(bool focused)
    {
        m_play.setInputFocused(focused);
    }

    // nullptr, если такого узла в сцене нет.
    Node* findNode(NodeId id);
    Node* selectedNode();

private:
    // Правка сцены и настроек идёт через общую историю.
    friend class SceneEditing;
    friend class SettingsEditing;
    friend class PrefabEditing;

    // Во время игры правка сцены в историю не попадает: по Stop сцена вернётся
    // как была. Правка настроек попадает всегда, она переживает Stop.
    bool execute(std::unique_ptr<Command> command);
    // Сцену сравнивают с файлом не каждый кадр: см. sceneModified.
    void updateModified(float deltaSeconds);
    // Уничтожает экземпляры модуля в сцене документа и выгружает его.
    void unloadGameModule();

    EngineServices& m_services;
    const SceneSerializer& m_serializer;
    GameModule& m_gameModule;
    std::optional<Project> m_project;
    // Есть, пока открыт проект.
    std::optional<GameLibrary> m_library;
    SceneDocument m_document;
    PlayMode m_play;
    Selection m_selection;
    // Выделение до Play: к нему возвращаемся, если после игры выбранных узлов не осталось.
    Selection m_selectionBeforePlay;
    EditHistory m_history;
    float m_sinceModifiedCheck = 0.0f;
    SceneEditing m_editing{*this};
    PrefabEditing m_prefabEditing{*this};
    PreviewLighting m_previewLighting{m_services};
    MaterialEditing m_materialEditing{*this};
    Pings m_pings;
    SettingsEditing m_settingsEditing{*this};
};

// Уточнение заголовка окна: проект и открытая сцена, "*" у несохранённой.
// Пусто, если проект не открыт.
std::string windowTitle(const EditorSession& session);

} // namespace yqlora::editor
