#pragma once

#include "yqlora/editor/ui/core/EditorPanel.hpp"

#include "yqlora/editor/ProjectFiles.hpp"

#include <filesystem>
#include <optional>
#include <string>

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
    // Пункты "Создать ресурс" для папки folder. Файл появляется только после
    // кнопки "Создать" в окне с именем: отмена ничего не оставляет.
    void drawNewResourceItems(const ProjectFiles& files, const std::filesystem::path& folder);
    // Снимает выделение файла и выбор ресурса в сессии.
    void clearSelection(EditorSession& session);
    void drawCreateDialog(EditorSession& session, const SceneSerializer& serializer, const EditorColors& colors);
    void drawRenameDialog(EditorSession& session, const EditorColors& colors);
    void drawDeleteDialog(EditorSession& session);
    // Папка, куда класть новые файлы: выбранная папка или папка выбранного файла.
    std::filesystem::path targetFolder() const;
    void startRename(const std::filesystem::path& path);

    ProjectFiles::Entry m_root;
    std::filesystem::path m_projectDirectory;
    std::filesystem::path m_selected;
    bool m_selectedIsDirectory = false;
    bool m_dirty = true;
    bool m_wasFocused = false;
    std::optional<std::filesystem::path> m_openRequest;

    bool m_renameRequested = false;
    std::filesystem::path m_renaming;
    std::string m_newName;
    bool m_createRequested = false;
    // Что создаёт открытое окно создания; nullptr - окно ещё не открывали.
    const NewResourceType* m_createType = nullptr;
    std::filesystem::path m_createFolder;
    std::string m_createName;
    bool m_deleteRequested = false;
    std::filesystem::path m_deleting;
    std::string m_error;
};

} // namespace yqlora::editor
