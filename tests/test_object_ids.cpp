#include "EngineRegistry.hpp"

#include <doctest/doctest.h>

#include <yqlora/core/EngineServices.hpp>
#include <yqlora/core/EngineTypes.hpp>
#include <yqlora/input/Input.hpp>
#include <yqlora/scene/MissingComponent.hpp>
#include <yqlora/scene/Node3D.hpp>
#include <yqlora/scene/ObjectId.hpp>
#include <yqlora/scene/Scene.hpp>
#include <yqlora/scene/SceneSerializer.hpp>

#include <nlohmann/json.hpp>

#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

using namespace yqlora;
using Json = nlohmann::json;

namespace {

struct Note final : Component {
    YQLORA_TYPE(Note, Component)
};

struct Fixture {
    Fixture()
    {
        registry.registerComponent<Note>().allowMultiple();
    }

    test::EngineRegistry registry;
    Input input;
    EngineServices services{input, registry};
    SceneSerializer serializer{registry};
    Scene scene{services};
};

// Миллион пар для проверки совпадений: несколько экземпляров на много узлов.
constexpr std::uint64_t kInstances = 1000;
constexpr std::uint64_t kSourcesPerInstance = 1000;

} // namespace

// Значения посчитаны отдельно (splitmix64 на Python): одинаковы на любой машине.
static_assert(deriveId(1, 2) == 0xe06dd043328bd285ULL);
static_assert(deriveId(2, 1) == 0xbcd9dbb49673066bULL);
static_assert(deriveId(0xda9dc314563ffd36ULL, 0xe51d90c4b7a2f386ULL) == 0xd17909f817ca482fULL);

TEST_CASE("deriveId: depends on the order, never 0, no collisions on a million pairs")
{
    CHECK(deriveId(1, 2) != deriveId(2, 1));
    CHECK(deriveId(0, 0) != 0);
    std::unordered_set<std::uint64_t> seen;
    seen.reserve(kInstances * kSourcesPerInstance);
    std::size_t zeros = 0;
    for (std::uint64_t instance = 1; instance <= kInstances; ++instance) {
        const std::uint64_t instanceId = deriveId(instance, 0);
        for (std::uint64_t source = 1; source <= kSourcesPerInstance; ++source) {
            const std::uint64_t id = deriveId(instanceId, source);
            zeros += id == 0 ? 1 : 0;
            seen.insert(id);
        }
    }
    CHECK(zeros == 0);
    CHECK(seen.size() == kInstances * kSourcesPerInstance);
}

TEST_CASE("newObjectId: never 0 and distinct")
{
    std::unordered_set<std::uint64_t> seen;
    for (int i = 0; i < 1000; ++i) {
        const std::uint64_t id = newObjectId();
        CHECK(id != 0);
        seen.insert(id);
    }
    CHECK(seen.size() == 1000);
}

TEST_CASE_FIXTURE(Fixture, "Component IDs: distinct on a node, kept through save, load and the Play copy")
{
    auto& root = scene.createRoot<Node3D>("Root");
    auto& first = root.addComponent<Note>();
    auto& second = root.addComponent<Note>();
    CHECK(first.id() != 0);
    CHECK(first.id() != second.id());
    CHECK(root.transform().id() != 0);

    // Копия Play и горячая перезагрузка идут через ту же сериализацию.
    Scene copy{services};
    REQUIRE(serializer.fromJson(copy, serializer.toJson(scene)));
    const auto notes = copy.root()->getComponents<Note>();
    REQUIRE(notes.size() == 2);
    CHECK(notes[0]->id() == first.id());
    CHECK(notes[1]->id() == second.id());
    CHECK(static_cast<Node3D*>(copy.root())->transform().id() == root.transform().id());
    // ID компонента не находит узел: это разные пространства.
    CHECK(copy.find(first.id()) == nullptr);
}

TEST_CASE_FIXTURE(Fixture, "Component IDs: a file without them or with a repeat gets new ones")
{
    auto& root = scene.createRoot<Node3D>("Root");
    root.addComponent<Note>();
    root.addComponent<Note>();
    Json json = Json::parse(serializer.toJson(scene));
    Json& components = json["root"]["components"];
    const auto noteAt = [&](std::size_t n) -> Json& {
        std::size_t seen = 0;
        for (Json& component : components) {
            if (component["type"] == "Note" && seen++ == n) {
                return component;
            }
        }
        FAIL("no such note");

        return components[0];
    };
    noteAt(0).erase("id");
    noteAt(1)["id"] = "00000000000000aa";
    // Повтор того же ID третьим компонентом.
    Json third = noteAt(1);
    components.push_back(third);

    Scene loaded{services};
    REQUIRE(serializer.fromJson(loaded, json.dump()));
    const auto notes = loaded.root()->getComponents<Note>();
    REQUIRE(notes.size() == 3);
    CHECK(notes[0]->id() != 0);
    CHECK(notes[1]->id() == 0xaa);
    CHECK(notes[2]->id() != 0xaa);
    CHECK(notes[0]->id() != notes[2]->id());
}

TEST_CASE_FIXTURE(Fixture, "Component IDs: a missing type keeps its ID and gives it back when it returns")
{
    TypeRegistry bare;
    registerEngineTypes(bare);
    SceneSerializer withoutNote(bare);
    auto& root = scene.createRoot<Node3D>("Root");
    const ComponentId id = root.addComponent<Note>().id();

    Scene loaded{services};
    REQUIRE(withoutNote.fromJson(loaded, serializer.toJson(scene)));
    const MissingComponent* missing = nullptr;
    for (const auto& component : loaded.root()->components()) {
        missing = missing ? missing : dynamic_cast<const MissingComponent*>(component.get());
    }
    REQUIRE(missing);
    CHECK(missing->id() == id);
    const std::string withMissing = withoutNote.toJson(loaded);

    // Модуль игры загрузился снова: заглушка оживает на месте с тем же ID.
    CHECK(serializer.reviveMissing(loaded) == 1);
    const auto alive = loaded.root()->getComponents<Note>();
    REQUIRE(alive.size() == 1);
    CHECK(alive[0]->id() == id);

    // Файл, записанный с заглушкой, читается с типом и тем же ID.
    Scene revived{services};
    REQUIRE(serializer.fromJson(revived, withMissing));
    const auto notes = revived.root()->getComponents<Note>();
    REQUIRE(notes.size() == 1);
    CHECK(notes[0]->id() == id);
}
