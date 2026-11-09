#include "yqlora/editor/ProjectFiles.hpp"

#include "yqlora/core/EngineServices.hpp"
#include "yqlora/core/Log.hpp"
#include "yqlora/core/Utf8Path.hpp"
#include "yqlora/editor/NewScene.hpp"
#include "yqlora/editor/ProjectSetup.hpp"
#include "yqlora/material/MaterialFile.hpp"
#include "yqlora/platform/Files.hpp"
#include "yqlora/project/Project.hpp"
#include "yqlora/resource/ResourceDatabase.hpp"
#include "yqlora/scene/Scene.hpp"
#include "yqlora/scene/SceneSerializer.hpp"
#include "yqlora/shader/ShaderData.hpp"

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
constexpr const char* kShaderTemplate = "shader/Shader.yshader";
constexpr const char* kSkyShaderTemplate = "sky/Sky.yshader";
// Файлы шаблона и метка имени класса в них; так же их читает tests/sdk/BuildTemplate.cmake.
constexpr const char* kTemplateFileName = "Script";
constexpr std::array<std::string_view, 2> kScriptExtensions{".hpp", ".cpp"};
constexpr std::string_view kNamePlaceholder = "$SCRIPT_NAME$";
constexpr std::size_t kMaxScriptNameLength = 64;

// "New Scene 2", "New Scene 3"...: первая копия имени получает номер 2.
constexpr int kFirstDuplicateNumber = 2;

// Файл, по которому видно, что имя name вида type занято.
std::string occupiedFileName(const NewResourceType& type, std::string_view name)
{
    return std::string(name) + std::string(type.extension);
}

const NewResourceType kNewResourceTypes[] = {
    {kFolderKind,
     "editor.files.newFolder",
     "New Folder",
     {},
     false,
     [](ProjectFiles& files, const fs::path& folder, std::string_view name, const NewResourceContext&) {
         return files.createFolder(folder, name);
     }},
    {SceneSerializer::kResourceKind,
     "editor.files.newScene",
     "New Scene",
     SceneSerializer::fileExtension(),
     false,
     [](ProjectFiles& files, const fs::path& folder, std::string_view name, const NewResourceContext& context) {
         return files.createScene(folder, name, context.serializer);
     }},
    {kCppScriptKind,
     "editor.files.newCppScript",
     "NewScript",
     kScriptExtensions.back(),
     true,
     [](ProjectFiles& files, const fs::path& folder, std::string_view name, const NewResourceContext& context) {
         return files.createScript(folder, name, cppScriptTemplateDirectory(context.assets));
     }},
    {ShaderData::kResourceKind,
     "editor.files.newShader",
     "New Shader",
     kShaderFileExtension,
     false,
     [](ProjectFiles& files, const fs::path& folder, std::string_view name, const NewResourceContext& context) {
         return files.createShader(folder, name, shaderTemplateFile(context.assets, ShaderKind::Surface));
     }},
    {kSkyShaderKind,
     "editor.files.newSkyShader",
     "New Sky",
     kShaderFileExtension,
     false,
     [](ProjectFiles& files, const fs::path& folder, std::string_view name, const NewResourceContext& context) {
         return files.createShader(folder, name, shaderTemplateFile(context.assets, ShaderKind::Sky));
     }},
    {MaterialData::kResourceKind,
     "editor.files.newMaterial",
     "New Material",
     kMaterialFileExtension,
     false,
     [](ProjectFiles& files, const fs::path& folder, std::string_view name, const NewResourceContext&) {
         return files.createMaterial(folder, name, MaterialData{});
     }},
};

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

const NewResourceType* findNewResourceType(std::string_view kind)
{
    const auto found = std::ranges::find(kNewResourceTypes, kind, &NewResourceType::kind);

    return found != std::end(kNewResourceTypes) ? &*found : nullptr;
}

fs::path cppScriptTemplateDirectory(const fs::path& assets)
{
    return assets / kTemplatesFolder / kCppScriptTemplate;
}

fs::path shaderTemplateFile(const fs::path& assets, ShaderKind kind)
{
    return assets / kTemplatesFolder / (kind == ShaderKind::Sky ? kSkyShaderTemplate : kShaderTemplate);
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

bool isValidResourceName(const NewResourceType& type, std::string_view name)
{
    return type.className ? isValidScriptName(name) : isValidFileName(occupiedFileName(type, name));
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

std::string ProjectFiles::suggestName(const NewResourceType& type, const fs::path& folder) const
{
    return suggestName(type, folder, type.defaultName);
}

std::string ProjectFiles::suggestName(const NewResourceType& type, const fs::path& folder, std::string_view base) const
{
    // У скрипта имя - имя класса, пробел в нём недопустим.
    const std::string separator = type.className ? "" : " ";
    const fs::path directory = m_project.directory() / folder;
    std::error_code ec;
    std::string name(base);
    for (int i = kFirstDuplicateNumber; fs::exists(directory / fromUtf8(occupiedFileName(type, name)), ec); ++i) {
        name = std::string(base) + separator + std::to_string(i);
    }

    return name;
}

std::optional<fs::path> ProjectFiles::createScene(
    const fs::path& folder,
    std::string_view name,
    const SceneSerializer& serializer
)
{
    const NewResourceType& type = *findNewResourceType(SceneSerializer::kResourceKind);
    const fs::path relative = folder / fromUtf8(occupiedFileName(type, name));
    if (!isValidResourceName(type, name) || !canModify(relative)) {
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
    if (!isValidResourceName(*findNewResourceType(kFolderKind), name) || !canModify(relative)) {
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
        auto templateText = platform::readFile(
            templateDirectory / (std::string(kTemplateFileName) + std::string(extension))
        );
        if (!templateText) {
            log::error("Script template is missing in '{}'", toUtf8(templateDirectory));

            return std::nullopt;
        }
        std::string text = std::move(*templateText);
        for (std::size_t at = text.find(kNamePlaceholder); at != std::string::npos;
             at = text.find(kNamePlaceholder, at + name.size())) {
            text.replace(at, kNamePlaceholder.size(), name);
        }
        files.emplace_back(target, std::move(text));
    }
    // Оба файла или ни одного: шаблон проверен до записи.
    for (const auto& [target, text] : files) {
        if (!platform::writeFileAtomically(target, text)) {
            log::error("Cannot write '{}'", toUtf8(target));

            return std::nullopt;
        }
    }

    return folder / fromUtf8(std::string(name) + std::string(kScriptExtensions.back()));
}

std::optional<fs::path> ProjectFiles::createShader(
    const fs::path& folder,
    std::string_view name,
    const fs::path& templateFile
)
{
    const NewResourceType& type = *findNewResourceType(ShaderData::kResourceKind);
    const fs::path relative = folder / fromUtf8(occupiedFileName(type, name));
    if (!isValidResourceName(type, name) || !canModify(relative)) {
        return std::nullopt;
    }
    std::error_code ec;
    if (fs::exists(m_project.directory() / relative, ec)) {
        return std::nullopt;
    }
    if (!fs::copy_file(templateFile, m_project.directory() / relative, ec)) {
        log::error("Cannot create shader '{}' from '{}': {}", name, toUtf8(templateFile), ec.message());

        return std::nullopt;
    }
    m_project.resources().add(relative);

    return relative;
}

std::optional<fs::path> ProjectFiles::createMaterial(
    const fs::path& folder,
    std::string_view name,
    const MaterialData& data
)
{
    const NewResourceType& type = *findNewResourceType(MaterialData::kResourceKind);
    const fs::path relative = folder / fromUtf8(occupiedFileName(type, name));
    std::error_code ec;
    if (!isValidResourceName(type, name) || !canModify(relative) || fs::exists(m_project.directory() / relative, ec)) {
        return std::nullopt;
    }
    if (!writeMaterialFile(m_project.directory() / relative, data)) {
        log::error("Cannot write material '{}'", toUtf8(relative));

        return std::nullopt;
    }
    m_project.resources().add(relative);

    return relative;
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
