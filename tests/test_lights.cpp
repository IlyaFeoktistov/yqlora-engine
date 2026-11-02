#include "EngineRegistry.hpp"

#include <doctest/doctest.h>

#include <yqlora/core/EngineServices.hpp>
#include <yqlora/input/Input.hpp>
#include <yqlora/render/Camera3D.hpp>
#include <yqlora/render/DirectionalLight3D.hpp>
#include <yqlora/render/EngineUniforms.hpp>
#include <yqlora/render/FrameBuilder.hpp>
#include <yqlora/render/LightingSettings.hpp>
#include <yqlora/render/PointLight3D.hpp>
#include <yqlora/render/RenderSystem.hpp>
#include <yqlora/render/SpotLight3D.hpp>
#include <yqlora/scene/Scene.hpp>

#include <cmath>
#include <string>

using namespace yqlora;

namespace {

struct Fixture {
    test::EngineRegistry registry;
    Input input;
    EngineServices services{input, registry};
    Scene scene{services};
};

constexpr float kAspect = 1.0f;

} // namespace

TEST_CASE("Lights: values stay in their ranges")
{
    SpotLight3D spot;
    spot.setOuterAngleDegrees(10.0f);
    CHECK(spot.innerAngleDegrees() <= spot.outerAngleDegrees());
    spot.setInnerAngleDegrees(40.0f);
    CHECK(spot.outerAngleDegrees() == doctest::Approx(40.0f));
    spot.setOuterAngleDegrees(120.0f);
    CHECK(spot.outerAngleDegrees() < 90.0f);

    PointLight3D point;
    point.setRange(-1.0f);
    CHECK(point.range() > 0.0f);
    point.setIntensity(-2.0f);
    CHECK(point.intensity() == 0.0f);
}

TEST_CASE_FIXTURE(Fixture, "Extract: lights of the scene in the world, inactive ones skipped")
{
    auto& root = scene.createRoot<Node3D>();
    root.addChild<Camera3D>("Camera");
    auto& sun = root.addChild<DirectionalLight3D>("Sun");
    sun.setColor(Vec3(1.0f, 0.5f, 0.0f));
    sun.setIntensity(2.0f);
    auto& lamp = root.addChild<PointLight3D>("Lamp");
    lamp.transform().setPosition({1.0f, 2.0f, 3.0f});
    lamp.setRange(4.0f);
    auto& off = root.addChild<SpotLight3D>("Off");
    off.setActive(false);

    const FrameData frame = extractFrame(scene, kAspect);
    REQUIRE(frame.lights.size() == 2);
    const LightData& directional = frame.lights[0];
    CHECK(directional.kind == LightKind::Directional);
    // Свет идёт вдоль -Z узла; цвет линейный и умножен на интенсивность.
    CHECK(math::nearlyEqual(directional.direction, Vec3(0.0f, 0.0f, -1.0f)));
    CHECK(math::nearlyEqual(directional.color, math::srgbToLinear(Vec3(1.0f, 0.5f, 0.0f)) * 2.0f));
    const LightData& point = frame.lights[1];
    CHECK(point.kind == LightKind::Point);
    CHECK(math::nearlyEqual(point.position, Vec3(1.0f, 2.0f, 3.0f)));
    CHECK(point.range == doctest::Approx(4.0f));

    // Окружающий свет без проекта - из настроек по умолчанию.
    CHECK(math::nearlyEqual(frame.ambient, LightingSettings{}.linearAmbient()));
}

TEST_CASE_FIXTURE(Fixture, "Extract: past the limits the nearest point lights to the first camera stay")
{
    auto& root = scene.createRoot<Node3D>();
    root.addChild<Camera3D>("Camera");
    for (std::size_t i = 0; i < kMaxDirectionalLights + 1; ++i) {
        root.addChild<DirectionalLight3D>("Sun " + std::to_string(i));
    }
    // Чем позже, тем ближе к камере в начале координат.
    for (std::size_t i = 0; i < kMaxPunctualLights + 2; ++i) {
        auto& light = root.addChild<PointLight3D>("Lamp " + std::to_string(i));
        light.transform().setPosition({0.0f, 0.0f, 100.0f - static_cast<float>(i)});
    }

    const FrameData frame = extractFrame(scene, kAspect);
    std::size_t directional = 0;
    float farthest = 0.0f;
    for (const LightData& light : frame.lights) {
        if (light.kind == LightKind::Directional) {
            ++directional;
        } else {
            farthest = std::max(farthest, light.position.z);
        }
    }
    CHECK(directional == kMaxDirectionalLights);
    CHECK(frame.lights.size() == kMaxDirectionalLights + kMaxPunctualLights);
    // Два самых дальних (100 и 99) отброшены.
    CHECK(farthest == doctest::Approx(98.0f));
    // Сообщение о лишних - один раз на сцену.
    CHECK_FALSE(scene.system<RenderSystem>()->takeLightLimitReport());
}

TEST_CASE("Frame block: lights by kind, a spot cone is 1 inside and 0 outside")
{
    FrameData frame;
    frame.ambient = Vec3(0.1f);
    frame.lights.push_back({.kind = LightKind::Directional, .color = Vec3(1.0f)});
    frame.lights.push_back({.kind = LightKind::Point, .range = 5.0f});
    LightData spot{.kind = LightKind::Spot, .range = 5.0f};
    spot.cosInner = std::cos(glm::radians(20.0f));
    spot.cosOuter = std::cos(glm::radians(30.0f));
    frame.lights.push_back(spot);

    const FrameUniforms uniforms = frameUniforms(frame);
    CHECK(uniforms.ambient == Vec3(0.1f));
    CHECK(uniforms.directionalCount == 1);
    REQUIRE(uniforms.punctualCount == 2);
    // У точечного конуса нет: множитель всегда 1.
    CHECK(uniforms.punctualLights[0].angleScale == 0.0f);
    CHECK(uniforms.punctualLights[0].angleOffset == 1.0f);
    const PunctualLightUniforms& cone = uniforms.punctualLights[1];
    const auto factor = [&](float cosine) {
        return cone.angleScale * cosine + cone.angleOffset;
    };
    CHECK(factor(spot.cosInner) == doctest::Approx(1.0f));
    CHECK(factor(spot.cosOuter) == doctest::Approx(0.0f));
}
