#include "yqlora/editor/ProjectFiles.hpp"

#include "yqlora/core/EngineServices.hpp"
#include "yqlora/core/Log.hpp"
#include "yqlora/core/Utf8Path.hpp"
#include "yqlora/editor/NewScene.hpp"
#include "yqlora/editor/ProjectSetup.hpp"
#include "yqlora/project/Project.hpp"
#include "yqlora/resource/ResourceDatabase.hpp"
#include "yqlora/scene/Scene.hpp"
#include "yqlora/scene/SceneSerializer.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <iterator>
#include <utility>

namespace yqlora::editor {

namespace {

namespace fs = std::filesystem;

constexpr const char* kTemplatesFolder = "templates";
constexpr const char* kCppScriptTemplate = "cpp-script";
// Файлы шаблона и метка имени класса в них; так же их читает tests/sdk/BuildTemplate.cmake.
constexpr const char* kTemplateFileName = "Script";
constexpr std::array<std::string_view, 2> kScriptExtensions{".hpp", ".cpp"};
constexpr std::string_view kNamePlaceholder = "$SCRIPT_NAME$";
constexpr std::size_t kMaxScriptNameLength = 64;

constexpr std::array<NewResourceType, 3> kNewResourceTypes{{
    {NewResourceKind::Folder, "editor.files.newFolder", "New Folder"},
    {NewResourceKind::Scene, "editor.files.newScene", "New Scene"},
    {NewResourceKind::CppScript, "editor.files.newCppScript", "NewScript"},
}};

// "New Scene 2", "New Scene 3"...: первая копия имени получает номер 2.
constexpr int kFirstDuplicateNumber = 2;

const NewResourceType& typeOf(NewResourceKind kind)
{
    return *std::ranges::find(kNewResourceTypes, kind, &NewResourceType::kind);
}

// Файл, по которому видно, что имя name вида kind занято.
std::string occupiedFileName(NewResourceKind kind, std::string_view name)
{
    switch (kind) {
    case NewResourceKind::Folder: return std::string(name);
    case NewResourceKind::Scene: return std::string(name) + std::string(SceneSerializer::fileExtension());
    case NewResourceKind::CppScript: return std::string(name) + std::string(kScriptExtensions.back());
    }

    return std::string(name);
}

void scanDirectory(const fs::path& root, const fs::path& relative, ProjectFiles::Entry& out)
{
    std::error_code ec;
    for (const fs::directory_entry& item : fs::directory_iterator(root / relative, ec)) {
        const std::string name = toUtf8(item.path().filename());
        if (name.empty() || name.front() == '.' || item.path().extension() == ResourceDatabase::metaExtension()
            || (relative.empty()
                && std::ranges::find(Project::reservedNames(), name) != Project::reservedNames().end())) {
            continue;
        }
        ProjectFiles::Entry entry;
        entry.name = name;
        entry.path = relative / item.path().filename();
        entry.directory = item.is_directory(ec);
        if (entry.directory) {
            scanDirectory(root, entry.path, entry);
        }
        out.children.push_back(std::move(entry));
    }
    std::ranges::sort(out.children, [](const ProjectFiles::Entry& a, const ProjectFiles::Entry& b) {
        if (a.directory != b.directory) {
            return a.directory;
        }

        return a.name < b.name;
    });
}

} // namespace

std::span<const NewResourceType> newResourceTypes()
{
    return kNewResourceTypes;
}

fs::path cppScriptTemplateDirectory(const fs::path& assets)
{
    return assets / kTemplatesFolder / kCppScriptTemplate;
}

bool isValidScriptName(std::string_view name)
{
    const auto isLatin = [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
    };
    if (name.empty() || name.size() > kMaxScriptNameLength || !isLatin(name.front())) {
        return false;
    }

    return std::ranges::all_of(name, [&](char c) {
        return isLatin(c) || (c >= '0' && c <= '9');
    });
}

bool isValidResourceName(NewResourceKind kind, std::string_view name)
{
    if (kind == NewResourceKind::CppScript) {
        return isValidScriptName(name);
    }

    return isValidFileName(occupiedFileName(kind, name));
}

ProjectFiles::ProjectFiles(Project& project)
    : m_project(project)
{
}

ProjectFiles::Entry ProjectFiles::scan() const
{
    Entry root;
    root.name = m_project.name();
    root.directory = true;
    scanDirectory(m_project.directory(), {}, root);

    return root;
}

bool ProjectFiles::isScene(const fs::path& path)
{
    return path.extension() == SceneSerializer::fileExtension();
}

bool ProjectFiles::canModify(const fs::path& path) const
{
    const fs::path normal = path.lexically_normal();
    if (normal.empty() || normal == "." || normal.is_absolute()) {
        return false;
    }
    // ".." в начале - путь выходит из папки проекта. Файл проекта и
    // настройки правит не панель файлов.
    const auto first = *normal.begin();
    const bool reserved = std::ranges::find(Project::reservedNames(), toUtf8(first)) != Project::reservedNames().end();

    return first != ".." && !reserved && !m_project.directory().empty();
}

std::string ProjectFiles::suggestName(NewResourceKind kind, const fs::path& folder) const
{
    const std::string base = typeOf(kind).defaultName;
    // У скрипта имя - имя класса, пробел в нём недопустим.
    const std::string separator = kind == NewResourceKind::CppScript ? "" : " ";
    const fs::path directory = m_project.directory() / folder;
    std::error_code ec;
    std::string name = base;
    for (int i = kFirstDuplicateNumber; fs::exists(directory / fromUtf8(occupiedFileName(kind, name)), ec); ++i) {
        name = base + separator + std::to_string(i);
    }

    return name;
}

std::optional<fs::path> ProjectFiles::createScene(
    const fs::path& folder,
    std::string_view name,
    const SceneSerializer& serializer
)
{
    const fs::path relative = folder / fromUtf8(occupiedFileName(NewResourceKind::Scene, name));
    if (!isValidResourceName(NewResourceKind::Scene, name) || !canModify(relative)) {
        return std::nullopt;
    }
    const fs::path file = m_project.directory() / relative;
    std::error_code ec;
    if (fs::exists(file, ec)) {
        return std::nullopt;
    }
    Scene scene(EngineServices::none());
    buildNewScene(scene, std::string(name));
    if (!serializer.save(scene, file)) {
        return std::nullopt;
    }
    m_project.resources().add(relative);

    return relative;
}

std::optional<fs::path> ProjectFiles::createFolder(const fs::path& folder, std::string_view name)
{
    const fs::path relative = folder / fromUtf8(std::string(name));
    if (!isValidResourceName(NewResourceKind::Folder, name) || !canModify(relative)) {
        return std::nullopt;
    }
    std::error_code ec;
    if (fs::exists(m_project.directory() / relative, ec)
        || !fs::create_directory(m_project.directory() / relative, ec)) {
        return std::nullopt;
    }

    return relative;
}

std::optional<fs::path> ProjectFiles::createScript(
    const fs::path& folder,
    std::string_view name,
    const fs::path& templateDirectory
)
{
    if ((!folder.empty() && !canModify(folder)) || !isValidScriptName(name)) {
        return std::nullopt;
    }
    std::error_code ec;
    std::vector<std::pair<fs::path, std::string>> files;
    for (const std::string_view extension : kScriptExtensions) {
        const fs::path target = m_project.directory() / folder / fromUtf8(std::string(name) + std::string(extension));
        if (fs::exists(target, ec)) {
            log::error("Cannot create script '{}': '{}' already exists", name, toUtf8(target.filename()));

            return std::nullopt;
        }
        std::ifstream in(
            templateDirectory / (std::string(kTemplateFileName) + std::string(extension)),
            std::ios::binary
        );
        if (!in) {
            log::error("Script template is missing in '{}'", toUtf8(templateDirectory));

            return std::nullopt;
        }
        std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        for (std::size_t at = text.find(kNamePlaceholder); at != std::string::npos;
             at = text.find(kNamePlaceholder, at + name.size())) {
            text.replace(at, kNamePlaceholder.size(), name);
        }
        files.emplace_back(target, std::move(text));
    }
    // Оба файла или ни одного: шаблон проверен до записи.
    for (const auto& [target, text] : files) {
        std::ofstream out(target, std::ios::binary);
        out << text;
        if (!out) {
            log::error("Cannot write '{}'", toUtf8(target));

            return std::nullopt;
        }
    }

    return folder / fromUtf8(std::string(name) + std::string(kScriptExtensions.back()));
}

std::optional<fs::path> ProjectFiles::rename(const fs::path& path, const std::string& newName)
{
    if (!canModify(path) || !isValidFileName(newName) || newName == Project::fileName()) {
        return std::nullopt;
    }
    const fs::path target = path.parent_path() / fromUtf8(newName);
    if (target.extension() == ResourceDatabase::metaExtension()) {
        return std::nullopt;
    }
    std::error_code ec;
    if (fs::exists(m_project.directory() / target, ec)) {
        return std::nullopt;
    }
    fs::rename(m_project.directory() / path, m_project.directory() / target, ec);
    if (ec) {
        log::error("Cannot rename '{}': {}", toUtf8(path), ec.message());

        return std::nullopt;
    }
    m_project.resources().moved(path, target);

    return target;
}

bool ProjectFiles::remove(const fs::path& path)
{
    if (!canModify(path)) {
        return false;
    }
    std::error_code ec;
    fs::remove_all(m_project.directory() / path, ec);
    if (ec) {
        log::error("Cannot delete '{}': {}", toUtf8(path), ec.message());

        return false;
    }
    m_project.resources().removed(path);

    return true;
}

} // namespace yqlora::editor
