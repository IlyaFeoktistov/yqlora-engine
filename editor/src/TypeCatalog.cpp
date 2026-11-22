#include "yqlora/editor/TypeCatalog.hpp"

#include "yqlora/core/Text.hpp"
#include "yqlora/core/Utf8Path.hpp"
#include "yqlora/editor/TypeTexts.hpp"
#include "yqlora/i18n/Localization.hpp"
#include "yqlora/i18n/Translate.hpp"
#include "yqlora/reflection/TypeCategories.hpp"
#include "yqlora/reflection/TypeRegistry.hpp"
#include "yqlora/scene/Node.hpp"

#include <algorithm>
#include <set>
#include <tuple>
#include <utility>

namespace yqlora::editor {

namespace {

constexpr std::string_view kCategoryKeyPrefix = "category.";
// Между частями пути в подписи полки.
constexpr std::string_view kShelfLabelSeparator = " > ";
constexpr std::string_view kParentFolder = "..";

std::vector<std::string_view> pathParts(std::string_view path)
{
    std::vector<std::string_view> parts;
    while (!path.empty()) {
        const std::size_t end = path.find(categories::kSeparator);
        parts.push_back(path.substr(0, end));
        path = end == std::string_view::npos ? std::string_view() : path.substr(end + 1);
    }

    return parts;
}

std::string englishOrEmpty(const Localization* english, const std::string& key)
{
    return english ? english->find(key).value_or(std::string()) : std::string();
}

// Подпись части пути полки компонентов: перевод category.<часть>, у папок
// скриптов - сама часть.
std::string categoryPartLabel(std::string_view part)
{
    return trOr(std::string(kCategoryKeyPrefix) + std::string(part), part);
}

// Полка скрипта без своей категории - папка его исходника в проекте под
// полкой скриптов; исходник в корне или вне проекта - сама полка скриптов.
std::string scriptShelf(const TypeInfo& type, const std::filesystem::path& projectDirectory)
{
    const std::string scripts(categories::kScripts);
    if (type.sourceFile().empty() || projectDirectory.empty()) {
        return scripts;
    }
    const std::filesystem::path folder = scriptSourcePath(type, projectDirectory).parent_path().lexically_normal();
    const std::filesystem::path relative = folder.lexically_relative(projectDirectory.lexically_normal());
    const std::string text = toUtf8(relative.generic_string());
    if (relative.empty() || text == "." || text.starts_with(kParentFolder)) {
        return scripts;
    }

    return scripts + categories::kSeparator + text;
}

std::string componentShelf(
    const TypeRegistry& registry,
    const TypeInfo& type,
    const std::filesystem::path& projectDirectory
)
{
    if (type.isScript() && type.category().empty()) {
        return scriptShelf(type, projectDirectory);
    }

    return std::string(categoryOf(registry, type));
}

// Полка узла - цепочка его базовых типов от корня: Camera3D - "Node/Node3D".
std::string nodeShelf(const TypeRegistry& registry, const TypeInfo& type)
{
    std::vector<const TypeInfo*> chain = registry.chain(type);
    std::string path;
    for (auto it = chain.rbegin(); it != chain.rend() && *it != &type; ++it) {
        if (!path.empty()) {
            path += categories::kSeparator;
        }
        path += (*it)->name();
    }

    return path;
}

std::string requiredNodeOf(const TypeRegistry& registry, const TypeInfo& type)
{
    for (const TypeInfo* t : registry.chain(type)) {
        if (!t->requiredNode().empty()) {
            return t->requiredNode();
        }
    }

    return {};
}

TypeEntry makeEntry(
    const TypeRegistry& registry,
    const TypeInfo& type,
    const std::filesystem::path& projectDirectory,
    const Localization* english
)
{
    TypeEntry entry;
    entry.type = &type;
    entry.name = typeDisplayName(type);
    entry.description = typeDescription(type, projectDirectory);
    entry.requiredNode = requiredNodeOf(registry, type);
    entry.guidePage = std::string(guidePageOf(registry, type));
    entry.searchText = type.name() + "\n" + entry.name + "\n" + entry.description + "\n"
                       + englishOrEmpty(english, type.nameKey()) + "\n"
                       + englishOrEmpty(english, type.descriptionKey());

    return entry;
}

void addShelfLabel(TypeEntry& entry, const std::vector<TypeShelf>& shelves)
{
    const auto shelf = std::ranges::find_if(shelves, [&](const TypeShelf& s) {
        return s.kind == ShelfKind::Category && s.path == entry.shelf;
    });
    if (shelf == shelves.end()) {
        return;
    }
    entry.shelfLabel = shelf->label;
    entry.searchText += "\n" + entry.shelfLabel;
}

} // namespace

TypeCatalog TypeCatalog::components(
    const TypeRegistry& registry,
    const Node& node,
    const std::filesystem::path& projectDirectory,
    const Localization* english
)
{
    TypeCatalog catalog;
    for (const auto& type : registry.types()) {
        if (type->kind() != TypeKind::Component || !type->creatable()) {
            continue;
        }
        TypeEntry entry = makeEntry(registry, *type, projectDirectory, english);
        entry.shelf = componentShelf(registry, *type, projectDirectory);
        entry.fit = checkAttach(registry, *type, node);
        if (entry.fit.problem == AttachProblem::None) {
            const auto missing = missingRequirements(registry, *type, node);
            for (const TypeInfo* required : missing.value_or(std::vector<const TypeInfo*>())) {
                entry.addsWith.push_back(typeDisplayName(*required));
            }
        }
        catalog.m_entries.push_back(std::move(entry));
    }
    catalog.addShelves(categoryPartLabel, true);

    return catalog;
}

TypeCatalog TypeCatalog::nodes(const TypeRegistry& registry, const Localization* english)
{
    TypeCatalog catalog;
    for (const auto& type : registry.types()) {
        if (type->kind() != TypeKind::Node || !type->creatable()) {
            continue;
        }
        TypeEntry entry = makeEntry(registry, *type, {}, english);
        entry.shelf = nodeShelf(registry, *type);
        catalog.m_entries.push_back(std::move(entry));
    }
    // Часть пути полки узлов - имя базового типа.
    catalog.addShelves(
        [&](std::string_view part) {
            return typeDisplayName(registry, std::string(part));
        },
        false
    );

    return catalog;
}

void TypeCatalog::addShelves(const ShelfPartLabel& partLabel, bool scriptsLast)
{
    std::set<std::string> paths;
    for (const TypeEntry& entry : m_entries) {
        std::string prefix;
        for (const std::string_view part : pathParts(entry.shelf)) {
            prefix += (prefix.empty() ? "" : std::string(1, categories::kSeparator)) + std::string(part);
            paths.insert(prefix);
        }
    }

    // Порядок дерева: соседи по подписи, полка скриптов последней.
    using SortKey = std::vector<std::pair<bool, std::string>>;
    std::vector<std::pair<SortKey, TypeShelf>> sorted;
    for (const std::string& path : paths) {
        SortKey key;
        std::string label;
        const std::vector<std::string_view> parts = pathParts(path);
        for (std::size_t i = 0; i < parts.size(); ++i) {
            const std::string part = partLabel(parts[i]);
            const bool last = scriptsLast && i == 0 && parts[i] == categories::kScripts;
            key.emplace_back(last, part);
            label += (label.empty() ? "" : std::string(kShelfLabelSeparator)) + part;
        }
        sorted.emplace_back(
            std::move(key),
            TypeShelf{
                .kind = ShelfKind::Category,
                .path = path,
                .label = std::move(label),
                .depth = static_cast<int>(parts.size()) - 1,
            }
        );
    }
    std::ranges::sort(sorted, {}, &std::pair<SortKey, TypeShelf>::first);

    m_shelves.push_back({.kind = ShelfKind::All, .path = {}, .label = tr("editor.typePicker.all"), .depth = 0});
    m_shelves.push_back({.kind = ShelfKind::Recent, .path = {}, .label = tr("editor.typePicker.recent"), .depth = 0});
    for (auto& [key, shelf] : sorted) {
        m_shelves.push_back(std::move(shelf));
    }
    for (TypeEntry& entry : m_entries) {
        addShelfLabel(entry, m_shelves);
    }
}

std::vector<const TypeEntry*> TypeCatalog::list(
    const TypeShelf& shelf,
    std::string_view search,
    std::span<const std::string> recent
) const
{
    std::vector<const TypeEntry*> result;
    if (search.empty() && shelf.kind == ShelfKind::Recent) {
        for (const std::string& name : recent) {
            if (const TypeEntry* entry = find(name)) {
                result.push_back(entry);
            }
        }

        return result;
    }
    const auto onShelf = [&](const TypeEntry& entry) {
        if (!search.empty()) {
            return containsIgnoringCase(entry.searchText, search);
        }
        if (shelf.kind == ShelfKind::All) {
            return true;
        }

        return entry.shelf == shelf.path
               || (entry.shelf.starts_with(shelf.path) && entry.shelf.size() > shelf.path.size()
                   && entry.shelf[shelf.path.size()] == categories::kSeparator);
    };
    for (const TypeEntry& entry : m_entries) {
        if (onShelf(entry)) {
            result.push_back(&entry);
        }
    }
    // TODO: сравнение по байтам UTF-8: "ё" встаёт после "я". Правильно -
    // сравнение по правилам языка (ICU collation), когда появится ICU.
    std::ranges::sort(result, [](const TypeEntry* a, const TypeEntry* b) {
        return std::tie(a->name, a->type->name()) < std::tie(b->name, b->type->name());
    });

    return result;
}

const TypeEntry* TypeCatalog::find(std::string_view typeName) const
{
    const auto found = std::ranges::find_if(m_entries, [&](const TypeEntry& entry) {
        return entry.type->name() == typeName;
    });

    return found != m_entries.end() ? &*found : nullptr;
}

std::string keptSelection(std::span<const TypeEntry* const> entries, const std::string& selected)
{
    const bool shown = std::ranges::any_of(entries, [&](const TypeEntry* entry) {
        return entry->type->name() == selected;
    });
    if (shown) {
        return selected;
    }

    return entries.empty() ? std::string() : entries.front()->type->name();
}

std::string steppedSelection(std::span<const TypeEntry* const> entries, const std::string& selected, int step)
{
    if (entries.empty()) {
        return {};
    }
    const auto current = std::ranges::find_if(entries, [&](const TypeEntry* entry) {
        return entry->type->name() == selected;
    });
    // Выбранного нет в списке - шаг считается от первой строки.
    const int index = current == entries.end() ? 0 : static_cast<int>(current - entries.begin());
    const int next = std::clamp(index + step, 0, static_cast<int>(entries.size()) - 1);

    return entries[static_cast<std::size_t>(next)]->type->name();
}

void rememberRecentType(std::vector<std::string>& recent, std::string_view typeName)
{
    std::erase(recent, typeName);
    recent.insert(recent.begin(), std::string(typeName));
    if (recent.size() > kRecentTypes) {
        recent.resize(kRecentTypes);
    }
}

std::string attachProblemText(const TypeRegistry& registry, const AttachCheck& check)
{
    switch (check.problem) {
    case AttachProblem::None: return {};
    case AttachProblem::WrongNode:
        return trFormat("editor.typePicker.wrongNode", typeDisplayName(registry, check.type));
    case AttachProblem::AlreadyAttached: return tr("editor.typePicker.alreadyAttached");
    case AttachProblem::RequirementUnavailable:
        return trFormat("editor.typePicker.requirementUnavailable", typeDisplayName(registry, check.type));
    }

    return {};
}

} // namespace yqlora::editor
