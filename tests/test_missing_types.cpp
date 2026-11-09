#include "EngineRegistry.hpp"

#include <doctest/doctest.h>

#include <yqlora/core/EngineServices.hpp>
#include <yqlora/core/JsonId.hpp>
#include <yqlora/scene/MissingComponent.hpp>
#include <yqlora/scene/Node3D.hpp>
#include <yqlora/scene/Scene.hpp>
#include <yqlora/scene/SceneSerializer.hpp>
#include <yqlora/scripting/ModuleRegistrar.hpp>
#include <yqlora/scripting/Script.hpp>

#include <nlohmann/json.hpp>

#include <string>

using namespace yqlora;
using Json = nlohmann::json;

namespace {

class Spinner : public Script {
public:
    YQLORA_SCRIPT(Spinner)

    float speed = 1.0f;

    YQLORA_FIELDS(field("speed", &Spinner::speed))
};

constexpr std::string_view kModule = "game";

// Сцена, где на корне скрипт Spinner и Transform3D; в обоих свойства, которых
// у типов нет. Встроенный Transform3D узел создаёт сам, поэтому после чтения
// он первый.
const Json kScene = Json::parse(R"({
    "version": 1,
    "root": {
        "type": "Node3D",
        "id": "00000000000000a1",
        "properties": {"name": "Root"},
        "components": [
            {"type": "Spinner", "properties": {"enabled": true, "speed": 4.0, "oldField": [1, 2]}, "extra": "kept"},
            {"type": "Transform3D", "properties": {"position": [0, 1, 0], "skew": 0.5}}
        ],
        "children": []
    }
})");

constexpr std::size_t kSpinnerInFile = 0;
constexpr std::size_t kTransformInFile = 1;
constexpr std::size_t kTransformLoaded = 0;
constexpr std::size_t kSpinnerLoaded = 1;

const Json& componentJson(const Json& scene, std::size_t index)
{
    return scene["root"]["components"][index];
}

struct Fixture {
    void registerGame()
    {
        ModuleRegistrar registrar(registry, std::string(kModule));
        registrar.registerScript<Spinner>();
    }

    Json saved() const
    {
        return Json::parse(serializer.toJson(scene));
    }

    test::EngineRegistry registry;
    SceneSerializer serializer{registry};
    Scene scene{EngineServices::none()};
};

} // namespace

TEST_CASE_FIXTURE(Fixture, "Missing type: the component is kept as read and written back unchanged")
{
    REQUIRE(serializer.fromJson(scene, kScene.dump()));
    const auto* missing = dynamic_cast<const MissingComponent*>(scene.root()->components()[kSpinnerLoaded].get());
    REQUIRE(missing);
    CHECK(missing->missingType() == "Spinner");
    // В списках сцены заглушка не выдаёт себя за пропавший тип.
    CHECK(scene.componentsOfType("Spinner").empty());

    // В файле ID не было: заглушка получила новый и пишет его, остальное как было.
    Json written = componentJson(saved(), kSpinnerLoaded);
    REQUIRE(written.contains("id"));
    CHECK(written["id"] == idToJson(missing->id()));
    written.erase("id");
    CHECK(written == componentJson(kScene, kSpinnerInFile));
}

TEST_CASE_FIXTURE(Fixture, "Missing type: properties a known type does not have are kept and written back")
{
    REQUIRE(serializer.fromJson(scene, kScene.dump()));
    const Json transform = componentJson(saved(), kTransformLoaded);
    CHECK(transform["properties"]["skew"] == 0.5);
    CHECK(transform["properties"]["position"] == componentJson(kScene, kTransformInFile)["properties"]["position"]);
}

TEST_CASE_FIXTURE(Fixture, "Missing type: the component comes back when its type is registered")
{
    REQUIRE(serializer.fromJson(scene, kScene.dump()));
    registerGame();
    CHECK(serializer.reviveMissing(scene) == 1);

    const auto spinners = scene.components<Spinner>();
    REQUIRE(spinners.size() == 1);
    CHECK(spinners.front()->speed == 4.0f);
    CHECK(scene.root()->components()[kSpinnerLoaded].get() == spinners.front());

    // Свойство, которого у скрипта нет, пережило оживление; ключ рядом со
    // свойствами - нет: это уже данные компонента, а не заглушки.
    const Json spinner = componentJson(saved(), kSpinnerLoaded);
    CHECK(spinner["properties"]["oldField"] == Json::array({1, 2}));
    CHECK(spinner["properties"]["speed"] == 4.0);
    CHECK_FALSE(spinner.contains("extra"));
    CHECK(serializer.reviveMissing(scene) == 0);
}
