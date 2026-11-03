#include "yqlora/editor/EditorSession.hpp"

#include "yqlora/editor/SceneCommands.hpp"

#include "yqlora/core/EngineServices.hpp"
#include "yqlora/core/Log.hpp"
#include "yqlora/core/Utf8Path.hpp"
#include "yqlora/project/GeneralSettings.hpp"
#include "yqlora/reflection/ComponentRules.hpp"
#include "yqlora/reflection/TypeRegistry.hpp"
#include "yqlora/resource/ResourceCache.hpp"
#include "yqlora/scene/Node3D.hpp"
#include "yqlora/scene/NodeTree.hpp"
#include "yqlora/scene/Scene.hpp"
#include "yqlora/scene/SceneSerializer.hpp"
#include "yqlora/scripting/GameModule.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

namespace yqlora::editor {

namespace {

// Как часто сцену сравнивают с файлом без правок в истории: заметить
// изменение в обход истории, не записывая сцену каждый кадр.
constexpr float kModifiedCheckSeconds = 0.5f;

} // namespace

EditorSession::EditorSession(EngineServices& services, const SceneSerializer& serializer, GameModule& gameModule)
    : m_services(services)
    , m_serializer(serializer)
    , m_gameModule(gameModule)
    , m_document(services, serializer)
    , m_play(serializer)
{
    // Редактор импортирует: вложенные ресурсы получают ID, .meta пишется.
    services.resources().setMetaPolicy(MetaPolicy::Import);
}

const TypeRegistry& EditorSession::registry() const
{
    return m_serializer.registry();
}

bool EditorSession::open(Project project, const std::filesystem::path& gameLibrary)
{
    close();
    m_project = std::move(project);
    m_services.setProject(&*m_project);
    m_copies.emplace(m_project->directory());
    m_copies->removeUnused();
    if (!gameLibrary.empty()) {
        loadGameLibrary(gameLibrary);
    } else {
        log::info("Project '{}' has no built game module yet", m_project->name());
    }
    // От прошлого проекта ничего не должно остаться, даже если стартовая
    // сцена не прочитается: тогда проект открывается с пустой сценой.
    const ResourceId startup = m_project->settings().get<GeneralSettings>().startupScene.id;
    m_document.reset(startup);
    if (!m_document.load(*m_project, startup)) {
        log::warn("Startup scene of '{}' is not loaded, starting with an empty scene", m_project->name());

        return false;
    }

    return true;
}

void EditorSession::close()
{
    stop();
    m_document.reset(0);
    m_history.clear();
    unloadGameModule();
    if (m_copies) {
        m_copies->removeUnused();
        m_copies.reset();
    }
    m_services.setProject(nullptr);
    m_project.reset();
    m_selection.clear();
}

bool EditorSession::reloadGameModule(const std::filesystem::path& gameLibrary)
{
    if (!m_project) {
        return false;
    }
    stop();
    m_history.finishEdit();
    // Поля скриптов пишет код старого модуля, пока он ещё загружен.
    const std::string scene = m_serializer.toJson(m_document.scene());
    if (m_history.canUndo() || m_history.canRedo()) {
        log::info("Edit history is cleared: the game module is reloaded");
    }
    m_history.clear();
    unloadGameModule();
    const bool loaded = loadGameLibrary(gameLibrary);
    // Без модуля скрипты возвращаются заглушками со всеми данными.
    if (!m_serializer.fromJson(m_document.scene(), scene)) {
        log::error("The scene is not restored after reloading the game module");
    }
    const std::vector<NodeId> selected(m_selection.nodes().begin(), m_selection.nodes().end());
    for (const NodeId id : selected) {
        if (!findNode(id)) {
            m_selection.deselect(id);
        }
    }
    if (loaded) {
        log::info("Game module reloaded");
    }

    return loaded;
}

bool EditorSession::loadGameLibrary(const std::filesystem::path& library)
{
    const auto copy = m_copies->copy(library);
    if (!copy || !m_gameModule.load(*copy)) {
        m_copies->removeUnused();

        return false;
    }
    m_gameLibrary = library;
    m_copies->removeUnused(*copy);

    return true;
}

void EditorSession::unloadGameModule()
{
    Scene* const scenes[] = {&m_document.scene()};
    m_gameModule.unload(scenes);
    m_gameLibrary.clear();
}

ResourceCache& EditorSession::resources() const
{
    return m_services.resources();
}

void EditorSession::refreshResources()
{
    if (!m_project) {
        return;
    }
    m_project->scanResources();
    m_services.resources().reloadChanged();
    // Выбранного файла могло не стать.
    if (m_selection.resource() != 0 && !m_project->resources().path(m_selection.resource())) {
        m_selection.selectResource(0);
    }
}

std::filesystem::path EditorSession::scenePath() const
{
    return m_project ? m_document.path(*m_project) : std::filesystem::path();
}

bool EditorSession::openScene(const std::filesystem::path& path)
{
    stop();
    const ResourceId id = m_project ? m_project->resources().id(path) : 0;
    if (id == 0 || !m_document.load(*m_project, id)) {
        return false;
    }
    m_history.clear();
    m_selection.clear();

    return true;
}

bool EditorSession::modified() const
{
    return sceneModified() || (m_project && m_project->settings().modified());
}

bool EditorSession::save()
{
    if (!m_project || playing() || !m_document.save(*m_project)) {
        return false;
    }

    return m_project->settings().saveModified();
}

bool EditorSession::canUndo() const
{
    return !playing() && m_history.canUndo();
}

bool EditorSession::canRedo() const
{
    return !playing() && m_history.canRedo();
}

bool EditorSession::undo()
{
    if (playing() || !m_history.undo(*this)) {
        return false;
    }
    m_document.refreshModified();

    return true;
}

bool EditorSession::redo()
{
    if (playing() || !m_history.redo(*this)) {
        return false;
    }
    m_document.refreshModified();

    return true;
}

void EditorSession::updateModified(float deltaSeconds)
{
    m_sinceModifiedCheck += deltaSeconds;
    if (m_sinceModifiedCheck >= kModifiedCheckSeconds) {
        m_sinceModifiedCheck = 0.0f;
        m_document.refreshModified();
    }
}

bool EditorSession::execute(std::unique_ptr<Command> command)
{
    if (playing() && command && command->changesScene()) {
        return command->apply(*this);
    }
    if (!m_history.execute(*this, std::move(command))) {
        return false;
    }
    m_document.refreshModified();

    return true;
}

void EditorSession::play()
{
    m_history.finishEdit();
    if (m_play.start(m_document.scene())) {
        m_selectionBeforePlay = m_selection;
    }
}

void EditorSession::stop()
{
    if (!playing()) {
        return;
    }
    const Selection selected = m_selection;
    m_play.stop();
    // Узлов, созданных в игре, в редактируемой сцене нет. Если не осталось
    // ни одного выбранного, возвращаем выделение, каким оно было до Play.
    m_selection.clear();
    for (const NodeId id : selected.nodes()) {
        if (findNode(id)) {
            m_selection.toggle(id);
        }
    }
    if (m_selection.empty()) {
        m_selection = m_selectionBeforePlay;
    }
}

Node* EditorSession::findNode(NodeId id)
{
    Scene& current = scene();

    return current.root() && id != 0 ? yqlora::findNode(*current.root(), id) : nullptr;
}

Node* EditorSession::selectedNode()
{
    return findNode(m_selection.selected());
}

std::string windowTitle(const EditorSession& session)
{
    if (!session.project()) {
        return {};
    }
    std::string title = session.project()->name() + " - " + toUtf8(session.scenePath().filename());
    if (session.modified()) {
        title += " *";
    }

    return title;
}

} // namespace yqlora::editor
