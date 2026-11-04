#include "EngineRegistry.hpp"

#include <doctest/doctest.h>

#include <yqlora/editor/PropertyValues.hpp>
#include <yqlora/reflection/TypeRegistry.hpp>
#include <yqlora/render/Camera3D.hpp>
#include <yqlora/scene/TimeSettings.hpp>

#include <algorithm>
#include <string>

using namespace yqlora;
using namespace yqlora::editor;

namespace {

const Property& propertyOf(const TypeRegistry& registry, const TypeInfo& type, std::string_view name)
{
    const auto properties = registry.properties(type);

    return **std::ranges::find(properties, name, &Property::name);
}

} // namespace

TEST_CASE("PropertyValues: the default is the value of a new object of the same type")
{
    test::EngineRegistry registry;
    const TypeInfo& cameraType = *registry.find<Camera3D>();

    Camera3D camera;
    camera.setFovDegrees(90.0f);
    camera.transform().setPosition({1.0f, 2.0f, 3.0f});
    const Property& fov = propertyOf(registry, cameraType, "fov");
    CHECK(defaultValue(registry, camera, fov) == PropertyValue{60.0f});

    // Transform создаёт сам узел: значение берётся у нового узла того же типа.
    const Component& transform = camera.transform();
    const Property& position = propertyOf(registry, *registry.find(transform.typeName()), "position");
    CHECK(defaultValue(registry, transform, position) == PropertyValue{Vec3(0.0f)});

    TimeSettings time;
    time.setFixedStep(0.5f);
    const Property& fixedStep = propertyOf(registry, *registry.find<TimeSettings>(), "fixedStep");
    CHECK(defaultValue(registry, time, fixedStep) == PropertyValue{kDefaultFixedStep});
}

TEST_CASE("PropertyValues: copied text pastes into a property of the same kind")
{
    test::EngineRegistry registry;
    const TypeInfo& cameraType = *registry.find<Camera3D>();
    const Property& clear = propertyOf(registry, cameraType, "clear");
    const Property& fov = propertyOf(registry, cameraType, "fov");
    const Property& name = propertyOf(registry, cameraType, "name");

    const auto depthOnly = EnumValue{static_cast<int>(CameraClear::DepthOnly)};
    const std::string text = valueToText(clear, depthOnly);
    CHECK(text == "\"DepthOnly\"");
    CHECK(valueFromText(clear, text) == PropertyValue{depthOnly});
    CHECK_FALSE(valueFromText(fov, text));
    CHECK(valueFromText(fov, "2.5") == PropertyValue{2.5f});

    // В строку вставляется и обычный текст.
    CHECK(valueFromText(name, "Player camera") == PropertyValue{std::string("Player camera")});
    CHECK(valueFromText(name, "\"Quoted\"") == PropertyValue{std::string("Quoted")});
}

TEST_CASE("PropertyValues: one number of a vector is copied and pasted alone")
{
    PropertyValue position{Vec3(1.0f, 2.0f, 3.0f)};
    const auto components = vectorComponents(position);
    REQUIRE(components.size() == 3);
    components[1] = *numberFromText(numberToText(5.5f));
    CHECK(position == PropertyValue{Vec3(1.0f, 5.5f, 3.0f)});

    // Три числа - не одно: в одно поле их не вставить.
    CHECK_FALSE(numberFromText("[1.0, 2.0, 3.0]"));
    CHECK_FALSE(numberFromText("text"));

    PropertyValue flag{true};
    CHECK(vectorComponents(flag).empty());
}
