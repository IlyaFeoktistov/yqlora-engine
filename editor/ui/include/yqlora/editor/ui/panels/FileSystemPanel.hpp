#pragma once

#include "yqlora/editor/ui/core/EditorPanel.hpp"
#include "yqlora/editor/ui/widgets/NameDialog.hpp"
#include "yqlora/editor/ui/widgets/ReleaseClick.hpp"

#include "yqlora/editor/ProjectFiles.hpp"
#include "yqlora/resource/ResourceId.hpp"
#include "yqlora/scene/NodeId.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace yqlora {

class SceneSerializer;

} // namespace yqlora

namespace yqlora::editor {

class EditorSession;
struct EditorColors;

// Файлы проекта внизу редактора: всё, что лежит в папке проекта, кроме
// служебного. "Создать ресурс" предлагает виды из newResourceTypes(). Двойной
// клик по сцене открывает её в редакторе, по другому файлу - программой
// системы (скрипт - в IDE); правый клик - меню с остальными действиями.
class FileSystemPanel final : public EditorPanel {
public:
    const char* id() const override
    {
        return "FileSystem";
    }

    const char* titleKey() const override
    {
        return "editor.window.files";
    }

    void draw(EditorContext& context) override;

    void onProjectOpened(EditorContext& /*context*/) override
    {
        m_dirty = true;
    }

private:
    void drawEntry(
        const ProjectFiles::Entry& entry,
        EditorSession& session,
        const SceneSerializer& serializer,
        const EditorColors& colors
    );
    void drawContextMenu(const ProjectFiles::Entry& entry, EditorSession& session);
    // Вложенные ресурсы строками под их файлом: их тянут в сцену или на поле,
    // по щелчку инспектор показывает их только для чтения.
    void drawNestedResources(ResourceId file, const std::filesystem::path& path, EditorSession& session);
    // Отметка нескольких файлов (Ctrl+щелчок): их тянут на поле-список разом.
    bool isMarked(ResourceId id) const;
    static std::uint64_t rowKey(const std::filesystem::path& path);
    void mark(ResourceId id);
    void dragResource(ResourceId id, const std::string& label);
    // Просьба показать файл (Pings): путь раскрывается, строка прокручивается
    // в вид и подсвечивается.
    void showIfPinged(ResourceId id);
    bool leadsToPinged(const ProjectFiles::Entry& entry) const;
    // Пункты "Создать ресурс" для папки folder. Файл появляется только после
    // кнопки "Создать" в окне с именем: отмена ничего не оставляет.
    void drawNewResourceItems(const ProjectFiles& files, const std::filesystem::path& folder);
    // Снимает выделение файла и выбор ресурса в сессии.
    void clearSelection(EditorSession& session);
    void drawCreateDialog(EditorSession& session, const SceneSerializer& serializer, const EditorColors& colors);
    void drawRenameDialog(EditorSession& session, const EditorColors& colors);
    // Вариант префаба или модели и префаб из узла, брошенного на папку: оба
    // через окно с именем.
    void drawVariantDialog(EditorSession& session, const EditorColors& colors);
    void drawBranchDialog(EditorSession& session, const EditorColors& colors);
    // Узел из дерева сцены отпущен над папкой folder: окно имени префаба.
    void acceptBranchDrop(EditorSession& session, const std::filesystem::path& folder);
    void drawDeleteDialog(EditorSession& session);
    // Папка, куда класть новые файлы: выбранная папка или папка выбранного файла.
    std::filesystem::path targetFolder() const;
    void startRename(const std::filesystem::path& path);

    ProjectFiles::Entry m_root;
    std::filesystem::path m_projectDirectory;
    // ResourceDatabase::filesRevision при последнем чтении папки.
    std::uint64_t m_filesRevision = 0;
    std::filesystem::path m_selected;
    bool m_selectedIsDirectory = false;
    bool m_dirty = true;
    bool m_wasFocused = false;
    std::optional<std::filesystem::path> m_openRequest;
    // Файл кода, который открыть в редакторе кода в конце кадра (от папки проекта).
    std::optional<std::filesystem::path> m_codeRequest;
    std::vector<ResourceId> m_marked;
    ReleaseClick m_release;
    ResourceId m_pinged = 0;
    std::filesystem::path m_pingedFile;
    double m_pingedAt = 0.0;
    bool m_scrollToPinged = false;

    std::filesystem::path m_renaming;
    NameDialog m_renameDialog;
    ResourceId m_variantBase = 0;
    std::filesystem::path m_variantFolder;
    NameDialog m_variantDialog;
    NodeId m_branchNode = 0;
    std::filesystem::path m_branchFolder;
    NameDialog m_branchDialog;
    // Что создаёт открытое окно создания; nullptr - окно ещё не открывали.
    const NewResourceType* m_createType = nullptr;
    std::filesystem::path m_createFolder;
    NameDialog m_createDialog;
    bool m_deleteRequested = false;
    std::filesystem::path m_deleting;
};

} // namespace yqlora::editor
