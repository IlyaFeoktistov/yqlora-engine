#include "yqlora/editor/ProjectSetup.hpp"

#include "yqlora/core/EngineServices.hpp"
#include "yqlora/core/Log.hpp"
#include "yqlora/core/Utf8Path.hpp"
#include "yqlora/editor/NewScene.hpp"
#include "yqlora/editor/ProjectFiles.hpp"
#include "yqlora/project/GeneralSettings.hpp"
#include "yqlora/render/MeshRenderer.hpp"
#include "yqlora/resource/BuiltinResources.hpp"
#include "yqlora/scene/Node3D.hpp"
#include "yqlora/scene/Scene.hpp"
#include "yqlora/scene/SceneSerializer.hpp"

#include <array>
#include <cctype>

namespace yqlora::editor {

namespace {

constexpr const char* kMainSceneName = "main";
constexpr const char* kScenesFolder = "scenes";
// Стартовые папки нового проекта; стартовая сцена - в kScenesFolder.
constexpr std::array<const char*, 3> kStarterFolders{kScenesFolder, "scripts", "assets"};
constexpr std::size_t kMaxFileNameLength = 64;
// Запрещены в именах файлов на Windows, а "/" - везде.
constexpr std::string_view kForbiddenFileNameChars = "/\\:*?\"<>|";

// Чтобы новый проект сразу что-то показывал: новая сцена и куб на сетке
// там, куда смотрит её камера.
void buildStarterScene(Scene& scene)
{
    buildNewScene(scene, "Main");
    auto& cube = scene.root()->addChild<Node3D>("Cube");
    cube.transform().setPosition(kNewSceneFocus);
    cube.addComponent<MeshRenderer>().setMesh(ResourceRef{builtin::kCubeMesh});
}

} // namespace

std::optional<Project> createProject(
    const std::filesystem::path& location,
    const std::string& name,
    const SceneSerializer& serializer,
    std::string& error
)
{
    if (!isValidFileName(name)) {
        error = "projects.error.invalidName";

        return std::nullopt;
    }
    const std::filesystem::path directory = location / fromUtf8(name);
    std::error_code ec;
    if (std::filesystem::exists(directory, ec)) {
        error = "projects.error.exists";

        return std::nullopt;
    }
    if (!std::filesystem::create_directories(directory, ec)) {
        error = "projects.error.cannotCreate";

        return std::nullopt;
    }

    for (const char* folder : kStarterFolders) {
        std::filesystem::create_directories(directory / folder, ec);
    }
    const std::filesystem::path mainScene = std::filesystem::path(kScenesFolder)
                                            / (std::string(kMainSceneName)
                                               + std::string(SceneSerializer::fileExtension()));
    Scene scene(EngineServices::none());
    buildStarterScene(scene);
    auto project = Project::create(directory, name, serializer.registry());
    if (!project || !serializer.save(scene, directory / mainScene)) {
        error = "projects.error.cannotCreate";

        return std::nullopt;
    }
    auto& general = project->settings().get<GeneralSettings>();
    general.gameName = project->name();
    general.startupScene = {project->resources().add(mainScene)};
    if (!project->settings().save(general)) {
        error = "projects.error.cannotCreate";

        return std::nullopt;
    }

    return project;
}

bool deleteProjectFolder(const std::filesystem::path& directory, std::string& error)
{
    std::error_code ec;
    if (!std::filesystem::is_regular_file(directory / Project::fileName(), ec)) {
        error = "projects.error.notProject";

        return false;
    }
    std::filesystem::remove_all(directory, ec);
    if (ec) {
        log::error("Cannot delete project folder '{}': {}", toUtf8(directory), ec.message());
        error = "projects.error.cannotDelete";

        return false;
    }

    return true;
}

bool isValidFileName(std::string_view name)
{
    if (name.empty() || name.size() > kMaxFileNameLength || name == "." || name == "..") {
        return false;
    }
    // Пробел или точка в конце: Windows их молча отбрасывает у имён папок.
    if (name.front() == ' ' || name.back() == ' ' || name.back() == '.') {
        return false;
    }
    for (const char c : name) {
        if (std::iscntrl(static_cast<unsigned char>(c)) || kForbiddenFileNameChars.find(c) != std::string_view::npos) {
            return false;
        }
    }

    return true;
}

} // namespace yqlora::editor
