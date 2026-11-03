#pragma once

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace yqlora {

class Project;
class SceneSerializer;

} // namespace yqlora

namespace yqlora::editor {

// Виды того, что создаётся в Файлах, но не ресурс движка.
inline constexpr std::string_view kFolderKind = "folder";
inline constexpr std::string_view kCppScriptKind = "cppScript";

class ProjectFiles;

// Что нужно созданию любого вида: сериализатор сцен и папка ресурсов
// редактора (assets) с шаблонами.
struct NewResourceContext {
    const SceneSerializer& serializer;
    std::filesystem::path assets;
};

// Что можно создать в панели файлов ("Создать ресурс"): строка таблицы
// newResourceTypes(). Новый вид (материал, скрипт Luau) - новая строка, меню
// и окно создания берут всё отсюда, значок - по kind.
struct NewResourceType {
    // Вид результата: T::kResourceKind, kFolderKind или kCppScriptKind.
    std::string_view kind;
    // Ключ перевода подписи.
    const char* labelKey;
    // Имя, которое предлагается в окне создания (у скрипта оно же имя класса).
    const char* defaultName;
    // Расширение файла, по которому видно, что имя занято; пусто - папка.
    std::string_view extension;
    // Имя - имя класса C++ (isValidScriptName), а не имя файла.
    bool className;
    // Создаёт name в папке folder: путь созданного (у скрипта - .cpp) или
    // nullopt, если имя не годится или занято.
    std::optional<std::filesystem::path> (*create)(
        ProjectFiles& files,
        const std::filesystem::path& folder,
        std::string_view name,
        const NewResourceContext& context
    );
};

std::span<const NewResourceType> newResourceTypes();

// nullptr, если такого вида в таблице нет.
const NewResourceType* findNewResourceType(std::string_view kind);

// Шаблон C++-скрипта среди ресурсов редактора (assets).
std::filesystem::path cppScriptTemplateDirectory(const std::filesystem::path& assets);

// Шаблон нового шейдера среди ресурсов редактора (assets).
std::filesystem::path shaderTemplateFile(const std::filesystem::path& assets);

// Годится как имя класса C++-скрипта: латиница, цифры и "_", не с цифры.
bool isValidScriptName(std::string_view name);

// Годится как имя нового ресурса вида type (без расширения).
bool isValidResourceName(const NewResourceType& type, std::string_view name);

// Файлы внутри папки проекта: дерево для панели файлов и операции над ним.
// Все пути - относительно папки проекта, выйти за её пределы нельзя.
// Вместе с файлами обновляются ресурсы проекта, файлы .meta переезжают следом.
class ProjectFiles {
public:
    struct Entry {
        std::string name;
        std::filesystem::path path;
        bool directory = false;
        std::vector<Entry> children;
    };

    explicit ProjectFiles(Project& project);

    static bool isScene(const std::filesystem::path& path);

    // Дерево файлов: папки первыми, внутри по алфавиту. Файл проекта, файлы
    // .meta и скрытые файлы (имя с точки) не показываются.
    Entry scan() const;

    // Свободное имя для нового ресурса вида type в папке folder: имя по
    // умолчанию, при занятом - с номером ("New Scene 2", "NewScript2").
    std::string suggestName(const NewResourceType& type, const std::filesystem::path& folder) const;

    // Новая пустая сцена name.yscene в папке folder. nullopt, если имя
    // не годится или занято.
    std::optional<std::filesystem::path> createScene(
        const std::filesystem::path& folder,
        std::string_view name,
        const SceneSerializer& serializer
    );

    // Новая папка name в папке folder. nullopt, если имя не годится или занято.
    std::optional<std::filesystem::path> createFolder(const std::filesystem::path& folder, std::string_view name);

    // C++-скрипт name в папке folder: name.hpp и name.cpp из шаблона
    // templateDirectory с подставленным именем класса. Скрипт сам
    // регистрируется в модуле игры (YQLORA_REGISTER_SCRIPT), список файлов
    // сборки находит его сам. Возвращает путь к .cpp; nullopt, если имя
    // не годится или файлы с ним уже есть (причина в логе).
    std::optional<std::filesystem::path> createScript(
        const std::filesystem::path& folder,
        std::string_view name,
        const std::filesystem::path& templateDirectory
    );

    // Шейдер name.yshader в папке folder, копия шаблона templateFile.
    // nullopt, если имя не годится, занято или шаблона нет (причина в логе).
    std::optional<std::filesystem::path> createShader(
        const std::filesystem::path& folder,
        std::string_view name,
        const std::filesystem::path& templateFile
    );

    // Переименовывает файл или папку в той же папке. Возвращает новый путь;
    // nullopt, если имя недопустимо, занято или путь трогать нельзя.
    std::optional<std::filesystem::path> rename(const std::filesystem::path& path, const std::string& newName);

    // Удаляет файл или папку со всем содержимым.
    bool remove(const std::filesystem::path& path);

    // Путь указывает внутрь проекта и это не сам файл проекта и не корень папки.
    bool canModify(const std::filesystem::path& path) const;

private:
    Project& m_project;
};

} // namespace yqlora::editor
