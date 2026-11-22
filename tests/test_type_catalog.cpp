#include <doctest/doctest.h>

#include <yqlora/core/EngineTypes.hpp>
#include <yqlora/editor/GuideLink.hpp>
#include <yqlora/editor/ScriptComment.hpp>
#include <yqlora/editor/TypeCatalog.hpp>
#include <yqlora/editor/ui/style/TypeIcons.hpp>
#include <yqlora/i18n/Localization.hpp>
#include <yqlora/reflection/TypeBuilder.hpp>
#include <yqlora/reflection/TypeCategories.hpp>
#include <yqlora/reflection/TypeRegistry.hpp>
#include <yqlora/scene/Node3D.hpp>
#include <yqlora/scripting/Script.hpp>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

using namespace yqlora;
using namespace yqlora::editor;

namespace {

// Скрипт "игры" с исходником в папке проекта.
class Guard final : public Script {
public:
    YQLORA_SCRIPT(Guard)
};

class Turret final : public Script {
public:
    YQLORA_SCRIPT(Turret)
};

// Язык на время теста: tr() отвечает на нём.
struct ActiveLanguage {
    explicit ActiveLanguage(const char* language)
        : localization(YQLORA_LOCALES_DIR)
    {
        REQUIRE(localization.load(language));
        Localization::setActive(&localization);
    }

    ~ActiveLanguage()
    {
        Localization::setActive(nullptr);
    }

    ActiveLanguage(const ActiveLanguage&) = delete;
    ActiveLanguage& operator=(const ActiveLanguage&) = delete;

    Localization localization;
};

bool isOffered(const TypeInfo& type)
{
    return (type.kind() == TypeKind::Component || type.kind() == TypeKind::Node) && type.creatable();
}

std::vector<std::string> names(const std::vector<const TypeEntry*>& entries)
{
    std::vector<std::string> result;
    for (const TypeEntry* entry : entries) {
        result.push_back(entry->type->name());
    }

    return result;
}

const TypeShelf& shelf(const TypeCatalog& catalog, ShelfKind kind, std::string_view path = {})
{
    const auto shelves = catalog.shelves();
    const auto found = std::ranges::find_if(shelves, [&](const TypeShelf& s) {
        return s.kind == kind && s.path == path;
    });
    REQUIRE(found != shelves.end());

    return *found;
}

} // namespace

TEST_CASE("Type picker: every engine component has a category, every offered type an icon and a name")
{
    TypeRegistry registry;
    registerEngineTypes(registry);
    Localization en(YQLORA_LOCALES_DIR);
    REQUIRE(en.load("en"));
    Localization ru(YQLORA_LOCALES_DIR);
    REQUIRE(ru.load("ru"));

    for (const auto& type : registry.types()) {
        if (!isOffered(*type)) {
            continue;
        }
        CAPTURE(type->name());
        CHECK(findTypeIcon(registry, *type) != nullptr);
        CHECK(en.has(type->nameKey()));
        CHECK(ru.has(type->nameKey()));
        CHECK(ru.translate(type->nameKey()) != en.translate(type->nameKey()));
        if (type->kind() != TypeKind::Component) {
            continue;
        }
        const std::string_view category = categoryOf(registry, *type);
        CHECK_FALSE(category.empty());
        // Каждая часть пути полки подписана на каждом языке.
        std::string_view rest = category;
        while (!rest.empty()) {
            const std::size_t end = rest.find(categories::kSeparator);
            const std::string key = "category." + std::string(rest.substr(0, end));
            CAPTURE(key);
            CHECK(en.has(key));
            CHECK(ru.has(key));
            rest = end == std::string_view::npos ? std::string_view() : rest.substr(end + 1);
        }
    }
}

TEST_CASE("Type picker: a shelf lists its types in name order, All lists every creatable component")
{
    TypeRegistry registry;
    registerEngineTypes(registry);
    Node3D node;
    const TypeCatalog catalog = TypeCatalog::components(registry, node, {}, nullptr);

    const std::vector<const TypeEntry*> all = catalog.list(shelf(catalog, ShelfKind::All), {}, {});
    const auto creatable = std::ranges::count_if(registry.types(), [](const auto& type) {
        return type->kind() == TypeKind::Component && type->creatable();
    });
    CHECK(static_cast<std::ptrdiff_t>(all.size()) == creatable);

    // Полка с вложенной показывает и её типы: на "Физике" и скрепления.
    const std::vector<const TypeEntry*> physics = catalog.list(
        shelf(catalog, ShelfKind::Category, categories::kPhysics),
        {},
        {}
    );
    CHECK(std::ranges::is_sorted(physics, {}, &TypeEntry::name));
    const std::vector<std::string> physicsNames = names(physics);
    CHECK(std::ranges::find(physicsNames, "RigidBody") != physicsNames.end());
    CHECK(std::ranges::find(physicsNames, "HingeJoint") != physicsNames.end());
    const std::vector<const TypeEntry*> joints = catalog.list(
        shelf(catalog, ShelfKind::Category, categories::kJoints),
        {},
        {}
    );
    const std::vector<std::string> jointNames = names(joints);
    CHECK(std::ranges::find(jointNames, "RigidBody") == jointNames.end());
    CHECK(std::ranges::is_sorted(joints, {}, &TypeEntry::name));

    // Полки узлов - дерево наследования: на полке Light3D - все источники света.
    const TypeCatalog nodes = TypeCatalog::nodes(registry, nullptr);
    const std::vector<std::string> lights = names(
        nodes.list(shelf(nodes, ShelfKind::Category, "Node/Node3D/Light3D"), {}, {})
    );
    CHECK(lights.size() == 3);
    CHECK(std::ranges::find(lights, "SpotLight3D") != lights.end());
}

TEST_CASE("Type picker: a type that does not fit the node is shown with its reason")
{
    TypeRegistry registry;
    registerEngineTypes(registry);
    Node plain;
    const TypeCatalog onPlain = TypeCatalog::components(registry, plain, {}, nullptr);
    const TypeEntry* renderer = onPlain.find("MeshRenderer");
    REQUIRE(renderer);
    CHECK(renderer->fit.problem == AttachProblem::WrongNode);
    CHECK(renderer->fit.type == "Node3D");
    CHECK_FALSE(attachProblemText(registry, renderer->fit).empty());
    // Петле нужно твёрдое тело, а ему - Node3D.
    const TypeEntry* hinge = onPlain.find("HingeJoint");
    REQUIRE(hinge);
    CHECK(hinge->fit.problem == AttachProblem::RequirementUnavailable);
    CHECK(hinge->fit.type == "RigidBody");

    Node3D body;
    body.addComponent(registry.find("RigidBody")->createComponent());
    const TypeCatalog onBody = TypeCatalog::components(registry, body, {}, nullptr);
    CHECK(onBody.find("RigidBody")->fit.problem == AttachProblem::AlreadyAttached);
    CHECK(onBody.find("HingeJoint")->fit.problem == AttachProblem::None);
    // Скреплений можно несколько, коллайдеров тоже.
    CHECK(onBody.find("BoxCollider")->fit.problem == AttachProblem::None);

    Node3D empty;
    const TypeCatalog onEmpty = TypeCatalog::components(registry, empty, {}, nullptr);
    const TypeEntry* addsBody = onEmpty.find("HingeJoint");
    REQUIRE(addsBody);
    CHECK(addsBody->addsWith.size() == 1);
    CHECK(addsBody->requiredNode.empty());
    CHECK(onEmpty.find("RigidBody")->requiredNode == "Node3D");
}

TEST_CASE("Type picker: search finds a type by a word of its Russian description and by its English name")
{
    TypeRegistry registry;
    registerEngineTypes(registry);
    const ActiveLanguage russian("ru");
    Localization english(YQLORA_LOCALES_DIR);
    REQUIRE(english.load("en"));
    Node3D node;
    const TypeCatalog catalog = TypeCatalog::components(registry, node, {}, &english);
    const TypeShelf& all = shelf(catalog, ShelfKind::All);

    // Слово из описания петли, другим регистром.
    const std::vector<std::string> doors = names(catalog.list(all, "ДВЕРИ", {}));
    CHECK(std::ranges::find(doors, "HingeJoint") != doors.end());
    // Английское имя при русском языке и имя типа.
    const std::vector<std::string> hinge = names(catalog.list(all, "hinge", {}));
    CHECK(std::ranges::find(hinge, "HingeJoint") != hinge.end());
    const std::vector<std::string> rigid = names(catalog.list(all, "Rigid body", {}));
    CHECK(rigid == std::vector<std::string>{"RigidBody"});
    // Поиск идёт по всем полкам, выбранная не важна.
    const std::vector<std::string> fromJoints = names(
        catalog.list(shelf(catalog, ShelfKind::Category, categories::kJoints), "таймер", {})
    );
    CHECK(fromJoints == std::vector<std::string>{"Timer"});
    CHECK(catalog.find("Timer")->shelfLabel == "Логика");
}

TEST_CASE("Type picker: an added type goes first in Recent, which keeps kRecentTypes entries")
{
    std::vector<std::string> recent;
    rememberRecentType(recent, "Timer");
    rememberRecentType(recent, "RigidBody");
    rememberRecentType(recent, "Timer");
    CHECK(recent == std::vector<std::string>{"Timer", "RigidBody"});
    for (std::size_t i = 0; i < kRecentTypes + 2; ++i) {
        rememberRecentType(recent, "Type" + std::to_string(i));
    }
    CHECK(recent.size() == kRecentTypes);
    CHECK(recent.front() == "Type" + std::to_string(kRecentTypes + 1));

    TypeRegistry registry;
    registerEngineTypes(registry);
    Node3D node;
    const TypeCatalog catalog = TypeCatalog::components(registry, node, {}, nullptr);
    // Порядок недавних - последний первым; типов, которых больше нет, не показывает.
    const std::vector<std::string> shown{"Timer", "Gone", "BoxCollider"};
    CHECK(
        names(catalog.list(shelf(catalog, ShelfKind::Recent), {}, shown))
        == std::vector<std::string>{"Timer", "BoxCollider"}
    );
}

TEST_CASE("Type picker: a script of the game is on the shelf of its folder, or its own")
{
    TypeRegistry registry;
    registerEngineTypes(registry);
    const std::filesystem::path project = std::filesystem::path("/projects/Game");
    registry.registerComponent<Guard>().script().allowMultiple().sourceFile("enemies/Guard.cpp").description("Patrols");
    registry.registerComponent<Turret>()
        .script()
        .allowMultiple()
        .sourceFile("/projects/Game/Turret.cpp")
        .category("Defence");
    Node3D node;
    const TypeCatalog catalog = TypeCatalog::components(registry, node, project, nullptr);

    const TypeEntry* guard = catalog.find("Guard");
    REQUIRE(guard);
    CHECK(guard->shelf == std::string(categories::kScripts) + "/enemies");
    CHECK(guard->description == "Patrols");
    CHECK(scriptSourcePath(*guard->type, project) == project / "enemies/Guard.cpp");
    CHECK(shelf(catalog, ShelfKind::Category, "Scripts/enemies").label.ends_with("enemies"));
    // Своя полка важнее папки.
    CHECK(catalog.find("Turret")->shelf == "Defence");
    // Полка скриптов - последней в списке полок.
    const auto shelves = catalog.shelves();
    const auto scripts = std::ranges::find_if(shelves, [](const TypeShelf& s) {
        return s.path == categories::kScripts;
    });
    REQUIRE(scripts != shelves.end());
    CHECK(std::ranges::all_of(scripts, shelves.end(), [](const TypeShelf& s) {
        return s.path.starts_with(categories::kScripts);
    }));
}

TEST_CASE("Script comment: the comment right above the class describes the script")
{
    const std::string header = "#pragma once\n"
                               "\n"
                               "// Not this one.\n"
                               "#include <x>\n"
                               "\n"
                               "// Walks between the points of its path\n"
                               "// and looks around at each.\n"
                               "//\n"
                               "/// Stops when it sees the player.\n"
                               "class GAME_API Guard : public yqlora::Script {\n"
                               "};\n"
                               "class Other;\n";
    CHECK(
        classComment(header, "Guard")
        == "Walks between the points of its path and looks around at each.\nStops when it sees the player."
    );
    CHECK(classComment(header, "Other").empty());
    CHECK(classComment(header, "Missing").empty());
    CHECK(classComment("struct Plain {};\n", "Plain").empty());
}

TEST_CASE("Guide link: the page in the language of the interface, English without a translation")
{
    CHECK(guideUrl("physics", "ru").ends_with("docs/guide/physics.ru.md"));
    CHECK(guideUrl("physics", "en").ends_with("docs/guide/physics.md"));
    CHECK(guideUrl("physics", "de").ends_with("docs/guide/physics.md"));
}
