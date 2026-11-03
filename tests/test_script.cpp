#include "EngineRegistry.hpp"

#include <doctest/doctest.h>

#include <yqlora/core/EngineServices.hpp>
#include <yqlora/input/Input.hpp>
#include <yqlora/input/InputSettings.hpp>
#include <yqlora/platform/InputFeeder.hpp>
#include <yqlora/reflection/PropertyJson.hpp>
#include <yqlora/resource/ResourceKinds.hpp>
#include <yqlora/scene/Node3D.hpp>
#include <yqlora/scene/Scene.hpp>
#include <yqlora/scene/SceneSerializer.hpp>
#include <yqlora/scene/Transform3D.hpp>
#include <yqlora/scripting/ModuleRegistrar.hpp>
#include <yqlora/scripting/Script.hpp>

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

using namespace yqlora;

namespace {

// Скрипт как в игре: движение по действию ввода, поле с пределами и закрытое поле.
class Mover : public Script {
public:
    YQLORA_SCRIPT(Mover)

    float speed = 2.0f;
    float runtimeOnly = 0.0f;
    // Ресурсы в полях скрипта: материал и список текстур.
    ResourceRef look;
    std::vector<ResourceRef> decals;

    YQLORA_FIELDS(
        field("speed", &Mover::speed).range(0.0f, 10.0f).formerly("moveSpeed").group("movement"),
        field("bonus", &Mover::m_bonus).displayName("Bonus").tooltip("Extra speed"),
        field("runtimeOnly", &Mover::runtimeOnly).notSaved().notShown(),
        field("look", &Mover::look).resource(kinds::kMaterial),
        field("decals", &Mover::decals).resource(kinds::kTexture)
    )

    int updates = 0;
    std::string found;

protected:
    void onUpdate(float deltaSeconds) override
    {
        ++updates;
        transform().setPosition(
            transform().position() + Vec3(input().actionValue("MoveX") * speed * deltaSeconds, 0.0f, 0.0f)
        );
        if (const Node* target = findNode("Target")) {
            found = target->name();
        }
    }

private:
    float m_bonus = 1.0f;
};

// Имя скрипта игры совпадает с типом движка.
class Camera3D : public Script {
public:
    YQLORA_SCRIPT(Camera3D)
};

constexpr std::string_view kGameModule = "game";

} // namespace

// Входная функция модуля, как в библиотеке игры; вне безымянного пространства
// имён, потому что она экспортируется.
YQLORA_MODULE_ENTRY(registrar)
{
    registrar.registerScript<Mover>().formerly("Walker").executionOrder(-1);
    registrar.registerScript<Camera3D>().formerly("Ignored");
}

namespace {

struct Fixture {
    Fixture()
    {
        ModuleRegistrar registrar(registry, std::string(kGameModule));
        yqloraModuleEntry(registrar);
        rejected.assign(registrar.rejected().begin(), registrar.rejected().end());
    }

    const Property& property(std::string_view name) const
    {
        const auto properties = registry.properties(*registry.find<Mover>());

        return **std::ranges::find(properties, name, &Property::name);
    }

    test::EngineRegistry registry;
    std::vector<RejectedType> rejected;
    Input input;
    EngineServices services{input, registry};
};

} // namespace

TEST_CASE_FIXTURE(Fixture, "Script: resource fields know their kind, like any property of the engine")
{
    CHECK(property("look").type() == PropertyType::Resource);
    CHECK(property("look").attributes().resourceKind == kinds::kMaterial);
    CHECK(property("decals").elementType() == PropertyType::Resource);
    CHECK(property("decals").attributes().resourceKind == kinds::kTexture);
}

TEST_CASE_FIXTURE(Fixture, "Script: registered as a script with its fields and their attributes")
{
    const TypeInfo& type = *registry.find<Mover>();
    CHECK(type.isScript());
    CHECK(type.allowsMultiple());
    CHECK(type.baseName() == "Script");
    CHECK_FALSE(registry.find<Script>()->creatable());

    const Property& speed = property("speed");
    CHECK(speed.attributes().min == 0.0f);
    CHECK(speed.attributes().max == 10.0f);
    CHECK(speed.attributes().group == "movement");
    CHECK(property("bonus").attributes().displayName == "Bonus");
    CHECK(property("bonus").attributes().tooltip == "Extra speed");
    CHECK_FALSE(property("runtimeOnly").attributes().saved);
    CHECK_FALSE(property("runtimeOnly").attributes().shown);

    // Закрытое поле читается и пишется так же, как открытое.
    Mover mover;
    CHECK(property("bonus").set(mover, PropertyValue{3.0f}));
    CHECK(property("bonus").get(mover) == PropertyValue{3.0f});
}

TEST_CASE_FIXTURE(Fixture, "Script: the module entry registers scripts under the module")
{
    const TypeInfo& type = *registry.find<Mover>();
    CHECK(type.module() == kGameModule);
    CHECK(type.executionOrder() == -1);
    CHECK(registry.find("Script")->module() == TypeRegistry::engineModule());

    // Тип со старым именем из файла находится, а пишется под новым.
    CHECK(registry.find("Walker") == &type);
    CHECK(Mover().typeName() == "Mover");

    CHECK(registry.removeModule(kGameModule) == 1);
    CHECK(registry.find("Walker") == nullptr);
}

TEST_CASE_FIXTURE(Fixture, "Script: a type name taken by another module is rejected, not replaced")
{
    REQUIRE(rejected.size() == 1);
    CHECK(rejected[0].name == "Camera3D");
    CHECK(rejected[0].takenBy == TypeRegistry::engineModule());
    CHECK(registry.find("Camera3D")->module() == TypeRegistry::engineModule());
    CHECK(registry.find("Ignored") == nullptr);

    // Тот же скрипт второй раз - имя занято уже своим модулем.
    ModuleRegistrar again(registry, std::string(kGameModule));
    again.registerScript<Mover>();
    REQUIRE(again.rejected().size() == 1);
    CHECK(again.rejected()[0].takenBy == kGameModule);
}

TEST_CASE_FIXTURE(Fixture, "Script: fields are saved by name, read under a former name, unsaved ones skipped")
{
    Mover mover;
    mover.speed = 4.0f;
    mover.runtimeOnly = 9.0f;
    const auto json = propertiesToJson(registry, *registry.find<Mover>(), mover);
    CHECK(json.contains("speed"));
    CHECK_FALSE(json.contains("runtimeOnly"));

    Mover loaded;
    propertiesFromJson(registry, *registry.find<Mover>(), loaded, nlohmann::json{{"moveSpeed", 7.0f}});
    CHECK(loaded.speed == 7.0f);
}

TEST_CASE_FIXTURE(Fixture, "Script: reaches the engine through its node and scene")
{
    InputSettings settings;
    InputFeeder feeder(input);
    feeder.keyDown(Key::D);
    feeder.updateActions(settings);

    Scene scene{services};
    auto& root = scene.createRoot<Node3D>("Root");
    root.addChild<Node3D>("Target");
    auto& mover = root.addComponent<Mover>();
    scene.advance(0.5f);

    CHECK(mover.updates == 1);
    CHECK(root.transform().position().x == doctest::Approx(1.0f));
    CHECK(mover.found == "Target");
}
