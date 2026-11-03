#include "yqlora/editor/ui/panels/FileSystemPanel.hpp"

#include "yqlora/editor/ui/style/Icons.hpp"
#include "yqlora/editor/ui/style/ResourceIcons.hpp"
#include "yqlora/editor/ui/style/Theme.hpp"
#include "yqlora/editor/ui/widgets/DragDrop.hpp"

#include "yqlora/core/AppPaths.hpp"
#include "yqlora/core/Utf8Path.hpp"
#include "yqlora/editor/EditorSession.hpp"
#include "yqlora/editor/ResourceChoices.hpp"
#include "yqlora/i18n/Translate.hpp"
#include "yqlora/platform/Platform.hpp"
#include "yqlora/project/Project.hpp"
#include "yqlora/resource/ResourceCache.hpp"
#include "yqlora/resource/ResourceDatabase.hpp"
#include "yqlora/resource/ResourceLoader.hpp"

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>

#include <cstdint>
#include <utility>

namespace yqlora::editor {

namespace {

constexpr float kNameFieldWidth = 360.0f;

// kind - вид ресурса файла (fileKind).
const char* iconFor(const ProjectFiles::Entry& entry, bool open, std::string_view kind)
{
    if (entry.directory) {
        return open ? icons::kFolderOpen : icons::kFolder;
    }
    if (kind.empty() && Project::isSource(entry.path)) {
        return resourceIcon(kCppScriptKind);
    }

    return resourceIcon(kind);
}

// Вложенные ресурсы строками под их файлом: их тянут в сцену или на поле.
void drawNestedResources(ResourceId file, const std::filesystem::path& path, EditorSession& session)
{
    ResourceCache& cache = session.resources();
    for (const ResourceId id : nestedResources(file, path, cache)) {
        const std::string_view key = session.project()->resources().key(id);
        const std::string label = std::string(resourceIcon(cache.loadedKind(id))) + "  "
                                  + std::string(subresourceLocalKey(key));
        ImGui::PushID(reinterpret_cast<const void*>(static_cast<std::uintptr_t>(id)));
        ImGui::TreeNodeEx(
            "##nested",
            ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth,
            "%s",
            label.c_str()
        );
        resourceDragSource(id, label);
        ImGui::PopID();
    }
}

constexpr const char* kNewResourcePopup = "##newResource";

// Кнопка с иконкой; текст - во всплывающей подсказке.
bool iconButton(const char* icon, const std::string& hint)
{
    const bool pressed = ImGui::Button(icon);
    ImGui::SetItemTooltip("%s", hint.c_str());

    return pressed;
}

} // namespace

void FileSystemPanel::draw(EditorContext& context)
{
    EditorSession& session = context.session;
    const SceneSerializer& serializer = context.serializer;
    if (!beginWindow(context) || !session.project()) {
        ImGui::End();

        return;
    }
    Project& project = *session.project();
    ProjectFiles files(project);

    // Перечитываем, когда открыли другой проект и когда панель получила фокус:
    // файлы могли поменять в проводнике.
    const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    if (project.directory() != m_projectDirectory || (focused && !m_wasFocused)) {
        m_projectDirectory = project.directory();
        m_dirty = true;
    }
    m_wasFocused = focused;
    if (m_dirty) {
        m_root = files.scan();
        m_dirty = false;
    }

    if (iconButton(icons::kAdd, tr("editor.files.newResource"))) {
        ImGui::OpenPopup(kNewResourcePopup);
    }
    if (ImGui::BeginPopup(kNewResourcePopup)) {
        drawNewResourceItems(files, targetFolder());
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    if (iconButton(icons::kRefresh, tr("editor.files.refresh"))) {
        m_dirty = true;
    }
    ImGui::Separator();

    if (ImGui::BeginChild("files")) {
        for (const auto& entry : m_root.children) {
            drawEntry(entry, session, serializer, context.theme.colors());
        }
        if (m_root.children.empty()) {
            ImGui::TextDisabled("%s", tr("editor.files.empty").c_str());
        }
    }
    ImGui::EndChild();

    drawCreateDialog(session, serializer, context.theme.colors());
    drawRenameDialog(session, context.theme.colors());
    drawDeleteDialog(session);
    ImGui::End();

    if (m_openRequest) {
        context.openScene(*std::exchange(m_openRequest, std::nullopt));
    }
}

void FileSystemPanel::drawEntry(
    const ProjectFiles::Entry& entry,
    EditorSession& session,
    const SceneSerializer& serializer,
    const EditorColors& colors
)
{
    const bool selected = entry.path == m_selected;
    const bool openScene = entry.path == session.scenePath();
    const auto startupScenePath = session.project()->startupScenePath();
    const bool mainScene = startupScenePath && entry.path == *startupScenePath;

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick
                               | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (!entry.directory) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }
    if (selected) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    // Открытая сцена выделена цветом, чтобы было видно, что сейчас правится.
    if (openScene) {
        ImGui::PushStyleColor(ImGuiCol_Text, toImVec4(colors.selectionOutline));
    }
    const std::string id = toUtf8(entry.path);
    const std::string_view kind = entry.directory ? std::string_view() : fileKind(entry.path, session.resources());
    const ResourceId resource = entry.directory ? 0 : session.project()->resources().id(entry.path);
    // Файл с вложенными ресурсами (модель) раскрывается в них.
    const bool nested = !entry.directory && !session.resources().nestedKindsOf(entry.path).empty();
    if (nested) {
        flags &= ~(ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen);
    }
    if (resource != 0 && resource == session.selection().resource()) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    // Иконка папки зависит от того, раскрыта ли она, а это известно до отрисовки узла.
    const bool expanded = entry.directory && ImGui::TreeNodeGetOpen(ImGui::GetID(id.c_str()));
    const char* icon = iconFor(entry, expanded, kind);
    const bool open = ImGui::TreeNodeEx(id.c_str(), flags, "%s  %s", icon, entry.name.c_str());
    if (openScene) {
        ImGui::PopStyleColor();
    }

    if (ImGui::IsItemClicked(ImGuiMouseButton_Left) || ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
        m_selected = entry.path;
        m_selectedIsDirectory = entry.directory;
        // Ресурс показывает инспектор; исходники и папки - не ресурсы.
        if (resource != 0) {
            session.selection().selectResource(resource);
        }
    }
    // Любой ресурс тянется: модель - в сцену, текстура - на поле текстуры.
    resourceDragSource(resource, std::string(icon) + "  " + entry.name);
    // Файл с вложенными ресурсами по двойному щелчку раскрывается, как папка.
    if (!entry.directory && !nested && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        if (ProjectFiles::isScene(entry.path)) {
            m_openRequest = entry.path;
        } else {
            platform::openWithSystem(session.project()->directory() / entry.path);
        }
    }
    // Меню привязывается к последнему элементу, поэтому сразу после строки файла.
    drawContextMenu(entry, session);
    if (mainScene) {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", icons::kMainScene);
        ImGui::SetItemTooltip("%s", tr("editor.files.mainScene").c_str());
    }

    if (entry.directory && open) {
        for (const auto& child : entry.children) {
            drawEntry(child, session, serializer, colors);
        }
        ImGui::TreePop();
    } else if (nested && open) {
        drawNestedResources(resource, entry.path, session);
        ImGui::TreePop();
    }
}

void FileSystemPanel::drawContextMenu(const ProjectFiles::Entry& entry, EditorSession& session)
{
    if (!ImGui::BeginPopupContextItem()) {
        return;
    }
    ProjectFiles files(*session.project());
    const auto mainScene = session.project()->startupScenePath();
    const auto item = [](const char* icon, const char* key) {
        const std::string label = std::string(icon) + "  " + tr(key);

        return ImGui::MenuItem(label.c_str());
    };

    if (entry.directory) {
        const std::string label = std::string(icons::kAdd) + "  " + tr("editor.files.newResource");
        if (ImGui::BeginMenu(label.c_str())) {
            drawNewResourceItems(files, entry.path);
            ImGui::EndMenu();
        }
        ImGui::Separator();
    } else if (ProjectFiles::isScene(entry.path)) {
        if (item(icons::kScene, "editor.files.open")) {
            m_openRequest = entry.path;
        }
        if (entry.path != mainScene && item(icons::kMainScene, "editor.files.setMain")) {
            session.settingsEditing().setStartupScene(entry.path);
        }
        ImGui::Separator();
    }
    if (item(icons::kRename, "editor.files.rename")) {
        startRename(entry.path);
    }
    if (item(icons::kDelete, "editor.files.delete")) {
        m_deleting = entry.path;
        m_deleteRequested = true;
    }
    ImGui::EndPopup();
}

void FileSystemPanel::drawNewResourceItems(const ProjectFiles& files, const std::filesystem::path& folder)
{
    for (const NewResourceType& type : newResourceTypes()) {
        const std::string label = std::string(resourceIcon(type.kind)) + "  " + tr(type.labelKey) + "...";
        if (!ImGui::MenuItem(label.c_str())) {
            continue;
        }
        m_createType = &type;
        m_createFolder = folder;
        m_createName = files.suggestName(type, folder);
        m_error.clear();
        m_createRequested = true;
    }
}

void FileSystemPanel::drawCreateDialog(
    EditorSession& session,
    const SceneSerializer& serializer,
    const EditorColors& colors
)
{
    if (!m_createType) {
        return;
    }
    const NewResourceType& type = *m_createType;
    const bool script = type.className;
    const std::string title = tr(type.labelKey) + "###NewResource";
    if (m_createRequested) {
        ImGui::OpenPopup(title.c_str());
        m_createRequested = false;
    }
    if (!ImGui::BeginPopupModal(title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }
    ImGui::TextUnformatted(tr(script ? "editor.files.scriptName" : "editor.files.name").c_str());
    if (ImGui::IsWindowAppearing()) {
        ImGui::SetKeyboardFocusHere();
    }
    ImGui::SetNextItemWidth(kNameFieldWidth);
    const bool entered = ImGui::InputText("##name", &m_createName, ImGuiInputTextFlags_EnterReturnsTrue);
    const bool valid = isValidResourceName(type, m_createName);
    if (!valid) {
        ImGui::TextColored(
            toImVec4(colors.errorText),
            "%s",
            tr(script ? "editor.files.error.scriptName" : "editor.files.error.name").c_str()
        );
    } else if (!m_error.empty()) {
        ImGui::TextColored(toImVec4(colors.errorText), "%s", tr(m_error).c_str());
    }

    ImGui::BeginDisabled(!valid);
    if (ImGui::Button(tr("editor.files.create").c_str()) || (entered && valid)) {
        ProjectFiles files(*session.project());
        const auto created = type.create(files, m_createFolder, m_createName, {serializer, assetsDirectory()});
        if (created) {
            m_selected = *created;
            m_selectedIsDirectory = type.extension.empty();
            m_dirty = true;
            ImGui::CloseCurrentPopup();
        } else {
            m_error = "editor.files.error.exists";
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button(tr("editor.common.cancel").c_str()) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void FileSystemPanel::drawRenameDialog(EditorSession& session, const EditorColors& colors)
{
    const std::string title = tr("editor.files.rename") + "###RenameFile";
    if (m_renameRequested) {
        ImGui::OpenPopup(title.c_str());
        m_renameRequested = false;
    }
    if (!ImGui::BeginPopupModal(title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }
    if (ImGui::IsWindowAppearing()) {
        ImGui::SetKeyboardFocusHere();
    }
    ImGui::SetNextItemWidth(kNameFieldWidth);
    const bool entered = ImGui::InputText("##name", &m_newName, ImGuiInputTextFlags_EnterReturnsTrue);
    if (!m_error.empty()) {
        ImGui::TextColored(toImVec4(colors.errorText), "%s", tr(m_error).c_str());
    }

    if (ImGui::Button(tr("editor.files.rename").c_str()) || entered) {
        if (const auto renamed = ProjectFiles(*session.project()).rename(m_renaming, m_newName)) {
            if (m_selected == m_renaming) {
                m_selected = *renamed;
            }
            m_dirty = true;
            ImGui::CloseCurrentPopup();
        } else {
            m_error = "editor.files.error.rename";
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(tr("editor.common.cancel").c_str()) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void FileSystemPanel::drawDeleteDialog(EditorSession& session)
{
    const std::string title = tr("editor.files.delete") + "###DeleteFile";
    if (m_deleteRequested) {
        ImGui::OpenPopup(title.c_str());
        m_deleteRequested = false;
    }
    if (!ImGui::BeginPopupModal(title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }
    ImGui::TextUnformatted(tr("editor.files.deleteQuestion").c_str());
    ImGui::TextDisabled("%s", toUtf8(m_deleting).c_str());
    ImGui::Separator();

    if (ImGui::Button(tr("editor.files.delete").c_str())) {
        ProjectFiles(*session.project()).remove(m_deleting);
        if (m_selected == m_deleting) {
            m_selected.clear();
        }
        m_dirty = true;
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button(tr("editor.common.cancel").c_str()) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

std::filesystem::path FileSystemPanel::targetFolder() const
{
    if (m_selected.empty()) {
        return {};
    }

    return m_selectedIsDirectory ? m_selected : m_selected.parent_path();
}

void FileSystemPanel::startRename(const std::filesystem::path& path)
{
    m_renaming = path;
    m_newName = toUtf8(path.filename());
    m_error.clear();
    m_renameRequested = true;
}

} // namespace yqlora::editor
