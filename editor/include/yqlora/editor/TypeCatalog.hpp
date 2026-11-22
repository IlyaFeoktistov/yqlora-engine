#pragma once

#include "yqlora/reflection/ComponentRules.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace yqlora {

class Localization;
class Node;
class TypeInfo;
class TypeRegistry;

} // namespace yqlora

namespace yqlora::editor {

// Сколько последних добавленных типов помнит полка "Недавние" у проекта.
inline constexpr std::size_t kRecentTypes = 8;

enum class ShelfKind : std::uint8_t {
    All,
    Recent,
    Category,
};

// Полка окна выбора типа.
struct TypeShelf {
    ShelfKind kind = ShelfKind::All;
    // У Category: путь категорий через "/", у узлов - цепочка базовых типов
    // от Node ("Node/Node3D").
    std::string path;
    std::string label;
    // Вложенность в дереве полок: 0 - верхняя.
    int depth = 0;
};

// Тип в окне выбора со всем, что о нём показывается.
struct TypeEntry {
    const TypeInfo* type = nullptr;
    // На языке человека; без перевода - имя типа.
    std::string name;
    // Полное описание; пусто - его нет.
    std::string description;
    // Путь полки и её подпись с родителями ("Физика > Скрепления").
    std::string shelf;
    std::string shelfLabel;
    // Почему тип не добавить к узлу; у узлов всегда подходит.
    AttachCheck fit;
    // Тип узла, на котором живёт компонент; пусто - на любом.
    std::string requiredNode;
    // Компоненты, которые добавятся вместе с ним, их имена на языке человека.
    std::vector<std::string> addsWith;
    // Страница руководства; пусто - нет.
    std::string guidePage;
    // По чему ищется: имена и описания на языке человека и на английском,
    // имя типа и подпись полки.
    std::string searchText;
};

// Что предлагает окно выбора типа: полки и типы на них. Собирается по
// реестру один раз, когда окно открывается; рисует его TypePickerDialog.
class TypeCatalog {
public:
    // Создаваемые компоненты для node, подходящие и нет (с причиной).
    // Скрипты стоят на полках папок своих исходников в projectDirectory.
    // english - английские тексты для поиска; nullptr - только текущий язык.
    static TypeCatalog components(
        const TypeRegistry& registry,
        const Node& node,
        const std::filesystem::path& projectDirectory,
        const Localization* english
    );

    // Создаваемые узлы; полки - дерево наследования.
    static TypeCatalog nodes(const TypeRegistry& registry, const Localization* english);

    // "Все", "Недавние", затем полки категорий деревом.
    std::span<const TypeShelf> shelves() const
    {
        return m_shelves;
    }

    // Типы полки по имени: у Category - её и вложенных полок, у All - все,
    // у Recent - по recent, последний добавленный первым. Непустой search -
    // совпадения со всех полок по имени.
    std::vector<const TypeEntry*> list(
        const TypeShelf& shelf,
        std::string_view search,
        std::span<const std::string> recent
    ) const;

    // nullptr, если такого типа в окне нет.
    const TypeEntry* find(std::string_view typeName) const;

private:
    // Подпись одной части пути полки.
    using ShelfPartLabel = std::function<std::string(std::string_view)>;

    // Полки по путям типов; scriptsLast - полка скриптов в конце списка.
    void addShelves(const ShelfPartLabel& partLabel, bool scriptsLast);

    std::vector<TypeShelf> m_shelves;
    std::vector<TypeEntry> m_entries;
};

// Тип добавлен: он первым в недавних, без повтора, их не больше kRecentTypes.
void rememberRecentType(std::vector<std::string>& recent, std::string_view typeName);

// Почему тип не подходит, на языке человека; пусто, если подходит.
std::string attachProblemText(const TypeRegistry& registry, const AttachCheck& check);

// Имя типа на языке человека (type.<имя>.name); без перевода - имя типа.
std::string typeDisplayName(const TypeInfo& type);

// Описание типа: перевод type.<имя>.description, иначе заданное при
// регистрации, иначе у скрипта - комментарий над его классом (исходник
// ищется от projectDirectory). Пусто - нет.
std::string typeDescription(const TypeInfo& type, const std::filesystem::path& projectDirectory);

// Исходник скрипта на диске: TypeInfo::sourceFile от папки проекта (так его
// пишет сборка модуля игры) или уже полный путь. Пусто у типов движка.
std::filesystem::path scriptSourcePath(const TypeInfo& type, const std::filesystem::path& projectDirectory);

} // namespace yqlora::editor
