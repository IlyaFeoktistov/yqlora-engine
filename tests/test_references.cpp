#include "EngineRegistry.hpp"

#include <doctest/doctest.h>

#include <yqlora/core/EngineServices.hpp>
#include <yqlora/editor/ReferenceFields.hpp>
#include <yqlora/input/Input.hpp>
#include <yqlora/material/MaterialData.hpp>
#include <yqlora/reflection/TypeRegistry.hpp>
#include <yqlora/render/DirectionalLight3D.hpp>
#include <yqlora/render/MeshRenderer.hpp>
#include <yqlora/render/PointLight3D.hpp>
#include <yqlora/render/SpotLight3D.hpp>
#include <yqlora/resource/BuiltinResources.hpp>
#include <yqlora/resource/ResourceCache.hpp>
#include <yqlora/scene/Node3D.hpp>
#include <yqlora/scene/Scene.hpp>
#include <yqlora/scene/SceneSerializer.hpp>
#include <yqlora/scripting/ModuleRegistrar.hpp>
#include <yqlora/scripting/Script.hpp>

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

using namespace yqlora;
using namespace yqlora::editor;

namespace {

// Скрипт с типизированными ссылками, как их пишет игра.
struct Door final : Script {
    YQLORA_TYPE(Door, Script)

    ComponentRef<MeshRenderer> shade;
    NodeRef lamp;
    Ref<MaterialData> openMaterial;
    std::vector<Ref<MaterialData>> variants;
    std::vector<NodeRef> lamps;

    YQLORA_FIELDS(
        field("shade", &Door::shade),
        field("lamp", &Door::lamp).node<PointLight3D>(),
        field("openMaterial", &Door::openMaterial).tooltip("used when open"),
        field("variants", &Door::variants),
        field("lamps", &Door::lamps).node<PointLight3D>()
    )
};

// Компонент движка с той же ссылкой, описанный строителем типа.
struct Holder final : Component {
    YQLORA_TYPE(Holder, Component)

    ComponentRef<MeshRenderer> target;
};

struct Fixture {
    Fixture()
    {
        ModuleRegistrar registrar(registry, "game");
        registrar.registerScript<Door>();
        registry.registerComponent<Holder>().property("target", &Holder::target);
    }

    const Property& property(std::string_view type, std::string_view name) const
    {
        const TypeInfo* info = registry.find(type);
        REQUIRE(info);
        const auto properties = registry.properties(*info);
        const auto it = std::ranges::find(properties, name, &Property::name);
        REQUIRE(it != properties.end());

        return **it;
    }

    test::EngineRegistry registry;
    Input input;
    EngineServices services{input, registry};
    SceneSerializer serializer{registry};
    Scene scene{services};
};

} // namespace

TEST_CASE_FIXTURE(Fixture, "Typed references: the type of the field gives the target, explicit attributes stay")
{
    const Property& shade = property("Door", "shade");
    CHECK(shade.type() == PropertyType::Component);
    CHECK(shade.attributes().componentType == "MeshRenderer");
    CHECK(property("Holder", "target").attributes().componentType == "MeshRenderer");

    const Property& open = property("Door", "openMaterial");
    CHECK(open.type() == PropertyType::Resource);
    // Вид - из Ref<MaterialData>, подсказка - из описания поля: обе на месте.
    CHECK(open.attributes().resourceKind == MaterialData::kResourceKind);
    CHECK(open.attributes().tooltip == "used when open");

    const Property& variants = property("Door", "variants");
    CHECK(variants.elementType() == PropertyType::Resource);
    CHECK(variants.attributes().resourceKind == MaterialData::kResourceKind);

    CHECK(property("Door", "lamp").attributes().nodeType == "PointLight3D");
    CHECK(property("Door", "lamps").attributes().nodeType == "PointLight3D");
}

TEST_CASE_FIXTURE(Fixture, "Typed references survive save, load and the Play copy, and resolve through the scene")
{
    auto& root = scene.createRoot<Node3D>("Root");
    auto& lamp = root.addChild<PointLight3D>("Lamp");
    auto& shade = lamp.addComponent<MeshRenderer>();
    auto& door = root.addChild<Node3D>("Door").addComponent<Door>();
    door.shade = {lamp.id(), shade.id()};
    door.lamp = NodeRef{lamp.id()};
    door.openMaterial = {builtin::kDefaultMaterial};
    door.lamps = {NodeRef{lamp.id()}};

    Scene copy{services};
    REQUIRE(serializer.fromJson(copy, serializer.toJson(scene)));
    const Door* loaded = copy.root()->findComponentInChildren<Door>();
    REQUIRE(loaded);
    CHECK(loaded->shade == door.shade);
    CHECK(loaded->openMaterial == door.openMaterial);
    CHECK(loaded->lamps == door.lamps);

    MeshRenderer* found = copy.find(loaded->shade);
    REQUIRE(found);
    CHECK(found->owner()->name() == "Lamp");
    CHECK(copy.find<PointLight3D>(loaded->lamp) != nullptr);
    CHECK(copy.find<SpotLight3D>(loaded->lamp) == nullptr);

    // Ушедший компонент делает ссылку пустой, а не висячей.
    Node* copiedLamp = copy.find(loaded->lamp);
    REQUIRE(copiedLamp);
    copiedLamp->removeComponent(*found);
    CHECK(copy.find(loaded->shade) == nullptr);
    copy.destroy(*copiedLamp);
    CHECK(copy.find(loaded->lamp) == nullptr);
}

TEST_CASE_FIXTURE(Fixture, "Drops: a node fits by its type, a component field takes the fitting component")
{
    auto& root = scene.createRoot<Node3D>("Root");
    auto& point = root.addChild<PointLight3D>("Point");
    auto& spot = root.addChild<SpotLight3D>("Spot");
    auto& sun = root.addChild<DirectionalLight3D>("Sun");
    auto& plain = root.addChild<Node3D>("Plain");
    auto& one = plain.addComponent<MeshRenderer>();

    const Property& lamp = property("Door", "lamp");
    CHECK(nodeFits(lamp, point, registry));
    // Прожектор - наследник точечного.
    CHECK(nodeFits(lamp, spot, registry));
    CHECK_FALSE(nodeFits(lamp, sun, registry));
    CHECK_FALSE(nodeFits(lamp, plain, registry));
    const auto lamps = nodeChoices(lamp, scene, registry);
    CHECK(lamps == std::vector<const Node*>{&point, &spot});

    const Property& shade = property("Door", "shade");
    CHECK(fittingComponents(shade, plain, registry) == std::vector<Component*>{&one});
    // Узла без подходящего компонента поле не берёт.
    CHECK(fittingComponents(shade, sun, registry).empty());
    CHECK(componentFits(shade, one, registry));
    CHECK_FALSE(componentFits(shade, plain.transform(), registry));
    CHECK(componentChoices(shade, scene, registry) == std::vector<const Component*>{&one});
    CHECK(componentLabel(one) == "Plain / MeshRenderer");
}

TEST_CASE_FIXTURE(Fixture, "Drops: several fitting components are asked, labels tell them apart")
{
    auto& root = scene.createRoot<Node3D>("Root");
    auto& holder = root.addChild<Node3D>("Holder");
    auto& first = holder.addComponent<Door>();
    auto& second = holder.addComponent<Door>();
    // Поле любого скрипта Door: на узле два, спросить какой.
    Property any = property("Door", "shade");
    any.attributes().componentType = "Door";
    const auto fitting = fittingComponents(any, holder, registry);
    CHECK(fitting == std::vector<Component*>{&first, &second});
    CHECK(componentLabel(first) == "Holder / Door");
    CHECK(componentLabel(second) == "Holder / Door 2");
}

TEST_CASE_FIXTURE(Fixture, "Drops onto lists: each fitting file or node becomes an element, the rest are skipped")
{
    auto& root = scene.createRoot<Node3D>("Root");
    auto& point = root.addChild<PointLight3D>("Point");
    auto& sun = root.addChild<DirectionalLight3D>("Sun");
    auto& spot = root.addChild<SpotLight3D>("Spot");

    const std::vector<const Node*> dropped{&point, &sun, &spot};
    const auto items = nodeItems(property("Door", "lamps"), dropped, registry);
    CHECK(items == std::vector<PropertyValue>{NodeRef{point.id()}, NodeRef{spot.id()}});

    const std::vector<ResourceId> files{builtin::kDefaultMaterial, builtin::kCubeMesh, builtin::kErrorMaterial};
    const auto materials = resourceItems(property("Door", "variants"), files, nullptr, services.resources());
    CHECK(
        materials
        == std::vector<PropertyValue>{ResourceRef{builtin::kDefaultMaterial}, ResourceRef{builtin::kErrorMaterial}}
    );
}
