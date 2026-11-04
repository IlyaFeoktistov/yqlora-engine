#include <doctest/doctest.h>

#include <yqlora/core/EngineTypes.hpp>
#include <yqlora/i18n/Localization.hpp>
#include <yqlora/reflection/TypeRegistry.hpp>
#include <yqlora/render/Camera3D.hpp>
#include <yqlora/scene/Node3D.hpp>
#include <yqlora/scene/Scene.hpp>
#include <yqlora/scene/SceneSerializer.hpp>

#include <string>
#include <vector>

using namespace yqlora;

namespace {

// Компонент, которого нет в движке: так тесты проверяют типы, зарегистрированные снаружи.
struct TestComponent final : Component {
    YQLORA_TYPE(TestComponent, Component)

    int value = 100;
};

// Компонент со списками: точки пути и имена.
struct ListComponent final : Component {
    YQLORA_TYPE(ListComponent, Component)

    std::vector<Vec3> points;
    std::vector<std::string> tags;
};

std::vector<std::string> names(const std::vector<const Property*>& properties)
{
    std::vector<std::string> result;
    for (const Property* property : properties) {
        result.push_back(property->name());
    }

    return result;
}

} // namespace

TEST_CASE("TypeRegistry: engine types form an inheritance tree")
{
    TypeRegistry registry;
    registerEngineTypes(registry);

    const TypeInfo* node = registry.find("Node");
    const TypeInfo* node3D = registry.find("Node3D");
    const TypeInfo* camera = registry.find("Camera3D");
    const TypeInfo* transform = registry.find("Transform3D");
    REQUIRE(node);
    REQUIRE(node3D);
    REQUIRE(camera);
    REQUIRE(transform);

    CHECK(node->baseName().empty());
    CHECK(node3D->baseName() == "Node");
    CHECK(camera->baseName() == "Node3D");
    CHECK(camera->kind() == TypeKind::Node);
    CHECK(transform->kind() == TypeKind::Component);
    CHECK(registry.find<Camera3D>() == camera);
    CHECK(registry.find("Missing") == nullptr);
}

TEST_CASE("TypeRegistry: nodes are created by type name, Transform is not")
{
    TypeRegistry registry;
    registerEngineTypes(registry);

    auto camera = registry.find("Camera3D")->createNode();
    REQUIRE(camera);
    CHECK(camera->typeName() == "Camera3D");

    // Transform создаёт сам узел, из меню или файла его не добавить.
    const TypeInfo* transform = registry.find("Transform3D");
    CHECK_FALSE(transform->creatable());
    CHECK(transform->createComponent() == nullptr);
}

TEST_CASE("TypeRegistry: properties include base types, base first")
{
    TypeRegistry registry;
    registerEngineTypes(registry);

    const auto properties = registry.properties(*registry.find("Camera3D"));
    CHECK(
        names(properties)
        == std::vector<std::string>{
            "name",
            "active",
            "layer",
            "fov",
            "near",
            "far",
            "order",
            "cullingMask",
            "clear",
            "background",
            "exposure",
            "toneMapping"
        }
    );
    // У компонентов корень - Component.
    CHECK(
        names(registry.properties(*registry.find("MeshRenderer")))
        == std::vector<std::string>{"enabled", "mesh", "materials", "castShadows", "receiveShadows"}
    );
    CHECK_FALSE(registry.find("Component")->creatable());
    // Встроенный компонент выключить нельзя.
    CHECK(
        names(registry.properties(*registry.find("Transform3D")))
        == std::vector<std::string>{"position", "rotation", "scale"}
    );
}

TEST_CASE("Property: reads and writes values through getters and setters")
{
    TypeRegistry registry;
    registerEngineTypes(registry);

    Camera3D camera("Main");
    const auto properties = registry.properties(*registry.find("Camera3D"));
    const Property& name = *properties[0];
    const Property& fov = *properties[3];

    CHECK(std::get<std::string>(name.get(camera)) == "Main");
    CHECK(fov.type() == PropertyType::Float);
    CHECK(fov.set(camera, 75.0f));
    CHECK(camera.fovDegrees() == doctest::Approx(75.0f));

    // Значение другого типа не применяется.
    CHECK_FALSE(fov.set(camera, std::string("wide")));
    CHECK(camera.fovDegrees() == doctest::Approx(75.0f));

    const Property& position = *registry.properties(*registry.find("Transform3D"))[0];
    CHECK(position.type() == PropertyType::Vector3);
    CHECK(position.set(camera.transform(), Vec3(1.0f, 2.0f, 3.0f)));
    CHECK(camera.transform().position() == Vec3(1.0f, 2.0f, 3.0f));
}

TEST_CASE("TypeRegistry: components registered outside the engine expose fields")
{
    TypeRegistry registry;
    registerEngineTypes(registry);
    registry.registerComponent<TestComponent>().property("value", &TestComponent::value);

    const TypeInfo* type = registry.find("TestComponent");
    REQUIRE(type);
    CHECK(type->creatable());

    Node3D node;
    auto& component = static_cast<TestComponent&>(node.addComponent(type->createComponent()));
    const Property& value = *type->ownProperties().data();
    CHECK(value.type() == PropertyType::Int);
    CHECK(value.set(component, 42));
    CHECK(component.value == 42);
    CHECK(std::get<int>(value.get(component)) == 42);
}

TEST_CASE("TypeRegistry: every engine type has a description in every language")
{
    TypeRegistry registry;
    registerEngineTypes(registry);

    Localization en(YQLORA_LOCALES_DIR);
    REQUIRE(en.load("en"));
    Localization ru(YQLORA_LOCALES_DIR);
    REQUIRE(ru.load("ru"));

    for (const auto& type : registry.types()) {
        const std::string key = type->descriptionKey();
        CAPTURE(key);
        CHECK(en.translate(key) != key);
        // Без своего перевода ru показал бы английский текст.
        CHECK(ru.translate(key) != en.translate(key));
    }
}

TEST_CASE("TypeRegistry: every engine property and enum value has a label in every language")
{
    TypeRegistry registry;
    registerEngineTypes(registry);
    Localization en(YQLORA_LOCALES_DIR);
    REQUIRE(en.load("en"));
    Localization ru(YQLORA_LOCALES_DIR);
    REQUIRE(ru.load("ru"));

    const auto check = [&](const std::string& key) {
        CAPTURE(key);
        CHECK(en.has(key));
        CHECK(ru.has(key));
    };
    const auto checkProperty = [&](const Property& property) {
        // Подпись из displayName уже задана самим свойством.
        if (property.attributes().displayName.empty()) {
            check(property.labelKey());
        }
        for (const std::string& value : property.enumNames()) {
            check(enumValueLabelKey(value));
        }
    };
    for (const auto& type : registry.types()) {
        for (const Property& property : type->ownProperties()) {
            if (!property.attributes().shown) {
                continue;
            }
            checkProperty(property);
            for (const Property& field : property.fields()) {
                checkProperty(field);
            }
        }
    }
}

namespace {

enum class Mood {
    Calm,
    Angry,
};

} // namespace

YQLORA_ENUM_NAMES(Mood, "Calm", "Angry")

namespace {

// Узел из "игры": свой модуль, перечисление и ссылки.
struct Guard final : Node3D {
    YQLORA_TYPE(Guard, Node3D)

    using Node3D::Node3D;

    Mood mood = Mood::Calm;
    NodeRef target;
    ResourceRef weapon;
};

} // namespace

TEST_CASE("TypeRegistry: the type name comes from YQLORA_TYPE")
{
    TypeRegistry registry;
    registerEngineTypes(registry);
    registry.registerNode<Guard>();

    const TypeInfo* guard = registry.find("Guard");
    REQUIRE(guard);
    CHECK(registry.find<Guard>() == guard);
    CHECK(guard->baseName() == "Node3D");
    CHECK(guard->createNode()->typeName() == "Guard");
}

TEST_CASE("TypeRegistry: types of a module go away together")
{
    TypeRegistry registry;
    registerEngineTypes(registry);
    const std::size_t engineTypes = registry.types().size();
    {
        TypeRegistry::ModuleScope scope(registry, "game");
        registry.registerNode<Guard>();
    }
    CHECK(registry.find("Guard")->module() == "game");
    CHECK(registry.find("Node3D")->module() == TypeRegistry::engineModule());

    CHECK(registry.removeModule("game") == 1);
    CHECK(registry.find("Guard") == nullptr);
    CHECK(registry.find<Guard>() == nullptr);
    CHECK(registry.types().size() == engineTypes);

    // Модуль можно зарегистрировать снова, как после перезагрузки dll.
    registry.registerNode<Guard>();
    CHECK(registry.find("Guard"));
}

TEST_CASE("Property: enums, node and resource references")
{
    TypeRegistry registry;
    registerEngineTypes(registry);
    registry.registerNode<Guard>()
        .property("mood", &Guard::mood)
        .property("target", &Guard::target)
        .property("weapon", &Guard::weapon);

    const auto properties = registry.properties(*registry.find("Guard"));
    const auto find = [&](std::string_view name) {
        return *std::ranges::find(properties, name, &Property::name);
    };
    const Property* mood = find("mood");
    CHECK(mood->type() == PropertyType::Enum);
    CHECK(mood->enumNames() == std::vector<std::string>{"Calm", "Angry"});
    CHECK(find("target")->type() == PropertyType::Node);
    CHECK(find("weapon")->type() == PropertyType::Resource);

    Guard guard;
    CHECK(mood->set(guard, EnumValue{1}));
    CHECK(guard.mood == Mood::Angry);
    CHECK(std::get<EnumValue>(mood->get(guard)).value == 1);
    find("target")->set(guard, NodeRef{42});
    CHECK(guard.target.id == 42);
}

TEST_CASE("Property: lists of any property type, checked and saved with the scene")
{
    TypeRegistry registry;
    registerEngineTypes(registry);
    registry.registerComponent<ListComponent>()
        .property("points", &ListComponent::points)
        .property("tags", &ListComponent::tags);
    const auto properties = registry.properties(*registry.find<ListComponent>());
    const Property& points = *properties[1];
    const Property& tags = *properties[2];
    CHECK(points.type() == PropertyType::List);
    CHECK(points.elementType() == PropertyType::Vector3);
    CHECK(tags.elementType() == PropertyType::String);

    ListComponent component;
    component.points = {{1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f}};
    const PropertyValue value = points.get(component);
    CHECK(std::get<ListValue>(value).items.size() == 2);

    // Элемент не того типа портит весь список.
    CHECK_FALSE(points.set(component, ListValue{{PropertyValue{1.0f}}}));
    CHECK(tags.set(component, ListValue{{PropertyValue{std::string("enemy")}}}));
    CHECK(component.tags == std::vector<std::string>{"enemy"});

    Scene scene{EngineServices::none()};
    scene.createRoot<Node>("Root").addComponent<ListComponent>().points = component.points;
    const SceneSerializer serializer(registry);
    Scene loaded{EngineServices::none()};
    REQUIRE(serializer.fromJson(loaded, serializer.toJson(scene)));
    const auto* loadedComponent = dynamic_cast<const ListComponent*>(loaded.root()->components().back().get());
    REQUIRE(loadedComponent);
    CHECK(loadedComponent->points == component.points);
    CHECK(defaultPropertyValue(PropertyType::List) == PropertyValue{ListValue{}});
}
