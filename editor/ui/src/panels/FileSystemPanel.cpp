#include "yqlora/editor/ui/panels/FileSystemPanel.hpp"

#include "yqlora/editor/ui/style/Icons.hpp"
#include "yqlora/editor/ui/style/Theme.hpp"
#include "yqlora/editor/ui/widgets/DragDrop.hpp"

#include "yqlora/core/AppPaths.hpp"
#include "yqlora/core/Utf8Path.hpp"
#include "yqlora/editor/EditorSession.hpp"
#include "yqlora/editor/ResourceLabel.hpp"
#include "yqlora/i18n/Translate.hpp"
#include "yqlora/import/ModelData.hpp"
#include "yqlora/platform/Platform.hpp"
#include "yqlora/project/Project.hpp"
#include "yqlora/render/TextureData.hpp"
#include "yqlora/resource/ResourceCache.hpp"
#include "yqlora/resource/ResourceDatabase.hpp"
#include "yqlora/shader/ShaderData.hpp"

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>

#include <utility>

namespace yqlora::editor {

namespace {

constexpr float kNameFieldWidth = 360.0f;

// kind - вид ресурса файла по его загрузчику (ResourceCache::kindOf).
const char* iconFor(const ProjectFiles::Entry& entry, bool open, std::string_view kind)
{
    if (entry.directory) {
        return open ? icons::kFolderOpen : icons::kFolder;
    }
    if (ProjectFiles::isScene(entry.path)) {
        return icons::kScene;
    }
    if (kind == ModelData::kResourceKind) {
        return icons::kModelFile;
    }
    if (kind == TextureData::kResourceKind) {
        return icons::kImageFile;
    }
    if (kind == ShaderData::kResourceKind) {
        return icons::kShaderFile;
    }

    return Project::isSource(entry.path) ? icons::kCodeFile : icons::kFile;
}

// Меши модели строками под её файлом: их тянут в сцену или на поле меша.
void drawModelMeshes(ResourceId file, EditorSession& session)
{
    const ModelData* model = session.resources().find<ModelData>(file);
    if (!model) {
        return;
    }
    const auto& ids = session.project()->resources().subresources(file);
    for (const std::string& key : model->meshes) {
        const auto id = ids.find(key);
        if (id == ids.end()) {
            continue;
        }
        const std::string label = std::string(icons::kMesh) + "  " + subresourceLabel(key);
        ImGui::TreeNodeEx(
            key.c_str(),
            ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth,
            "%s",
            label.c_str()
        );
        resourceDragSource(id->second, label);
    }
}

const char* iconFor(NewResourceKind kind)
{
    switch (kind) {
    case NewResourceKind::Folder: return icons::kAddFolder;
    case NewResourceKind::Scene: return icons::kScene;
    case NewResourceKind::CppScript: return icons::kCodeFile;
    case NewResourceKind::Shader: return icons::kShaderFile;
    }

    return icons::kFile;
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
    const std::string_view kind = entry.directory ? std::string_view() : session.resources().kindOf(entry.path);
    const ResourceId resource = entry.directory ? 0 : session.project()->resources().id(entry.path);
    // Модель раскрывается в свои меши.
    const bool model = kind == ModelData::kResourceKind;
    if (model) {
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
    // Модель по двойному щелчку раскрывается, как папка.
    if (!entry.directory && !model && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
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
    } else if (model && open) {
        drawModelMeshes(resource, session);
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
        const std::string label = std::string(iconFor(type.kind)) + "  " + tr(type.labelKey) + "...";
        if (!ImGui::MenuItem(label.c_str())) {
            continue;
        }
        m_createKind = type.kind;
        m_createFolder = folder;
        m_createName = files.suggestName(type.kind, folder);
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
    const bool script = m_createKind == NewResourceKind::CppScript;
    const auto& types = newResourceTypes();
    const auto type = std::ranges::find(types, m_createKind, &NewResourceType::kind);
    const std::string title = tr(type->labelKey) + "###NewResource";
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
    const bool valid = isValidResourceName(m_createKind, m_createName);
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
        std::optional<std::filesystem::path> created;
        switch (m_createKind) {
        case NewResourceKind::Folder: created = files.createFolder(m_createFolder, m_createName); break;
        case NewResourceKind::Scene: created = files.createScene(m_createFolder, m_createName, serializer); break;
        case NewResourceKind::CppScript:
            created = files.createScript(m_createFolder, m_createName, cppScriptTemplateDirectory(assetsDirectory()));
            break;
        case NewResourceKind::Shader:
            created = files.createShader(m_createFolder, m_createName, shaderTemplateFile(assetsDirectory()));
            break;
        }
        if (created) {
            m_selected = *created;
            m_selectedIsDirectory = m_createKind == NewResourceKind::Folder;
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
