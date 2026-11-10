#include "EngineRegistry.hpp"

#include <doctest/doctest.h>

#include <yqlora/core/EngineServices.hpp>
#include <yqlora/editor/NewScene.hpp>
#include <yqlora/input/Input.hpp>
#include <yqlora/render/Camera3D.hpp>
#include <yqlora/render/DirectionalLight3D.hpp>
#include <yqlora/render/EngineUniforms.hpp>
#include <yqlora/render/Environment.hpp>
#include <yqlora/render/FrameBuilder.hpp>
#include <yqlora/render/FrameExtract.hpp>
#include <yqlora/render/PointLight3D.hpp>
#include <yqlora/render/RenderSystem.hpp>
#include <yqlora/render/SpotLight3D.hpp>
#include <yqlora/resource/BuiltinResources.hpp>
#include <yqlora/scene/Scene.hpp>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

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

    // Без узла Environment - ни неба, ни атмосферы.
    CHECK_FALSE(frame.environment.sky);
    CHECK_FALSE(frame.environment.atmosphere.enabled);
}

TEST_CASE_FIXTURE(Fixture, "Extract: the flagged directional light is the sun, without a flag the brightest")
{
    auto& root = scene.createRoot<Node3D>();
    auto& dim = root.addChild<DirectionalLight3D>("Dim");
    dim.setIntensity(10.0f);
    auto& bright = root.addChild<DirectionalLight3D>("Bright");
    bright.setIntensity(1000.0f);
    root.addChild<PointLight3D>("Lamp");

    const auto sunOf = [&] {
        const FrameData frame = extractFrame(scene, kAspect);
        REQUIRE(frame.lights.size() >= 2);
        // Солнце - первым, и оно одно.
        CHECK(frame.lights[0].sun);
        CHECK(frame.lights[0].shadows == ShadowMode::Soft);
        CHECK(std::ranges::count_if(frame.lights, &LightData::sun) == 1);
        const FrameUniforms uniforms = frameUniforms(frame);
        CHECK(uniforms.hasSun == 1);
        CHECK(uniforms.sunColor == frame.lights[0].color);

        return frame.lights[0].color.r;
    };
    CHECK(sunOf() == doctest::Approx(1000.0f));
    dim.setSun(true);
    CHECK(sunOf() == doctest::Approx(10.0f));
    dim.setActive(false);
    CHECK(sunOf() == doctest::Approx(1000.0f));
}

TEST_CASE_FIXTURE(Fixture, "Extract: the first active environment gives the sky and the atmosphere")
{
    auto& root = scene.createRoot<Node3D>();
    auto& off = root.addChild<Environment>("Off");
    off.setActive(false);
    auto& environment = root.addChild<Environment>("Environment");
    environment.atmosphere.mieAnisotropy = 0.5f;

    FrameData frame = extractFrame(scene, kAspect);
    REQUIRE(frame.environment.sky);
    const FrameMaterial& sky = frame.materials[*frame.environment.sky];
    CHECK(sky.id == builtin::kDefaultSkyMaterial);
    CHECK(sky.shader->kind == ShaderKind::Sky);
    CHECK(frame.environment.atmosphere.enabled);
    CHECK(frame.environment.atmosphere.mieAnisotropy == 0.5f);

    // Материал не неба в небе пропускается, и об этом один раз сказано.
    environment.sky = ResourceRef{builtin::kDefaultMaterial};
    frame = extractFrame(scene, kAspect);
    CHECK_FALSE(frame.environment.sky);
    CHECK_FALSE(scene.system<RenderSystem>()->takeReport(RenderReport::SkyMaterial));
    environment.sky = {};
    CHECK_FALSE(extractFrame(scene, kAspect).environment.sky);
}

TEST_CASE_FIXTURE(Fixture, "Extract: a spot light has its range and the cosines of its cone")
{
    auto& root = scene.createRoot<Node3D>();
    auto& spot = root.addChild<SpotLight3D>("Spot");
    spot.setRange(6.0f);
    spot.setOuterAngleDegrees(40.0f);
    spot.setInnerAngleDegrees(25.0f);

    const std::vector<LightData> lights = extractLights(*scene.system<RenderSystem>(), Vec3(0.0f));
    REQUIRE(lights.size() == 1);
    CHECK(lights[0].kind == LightKind::Spot);
    CHECK(lights[0].range == doctest::Approx(6.0f));
    CHECK(lights[0].cosInner == doctest::Approx(std::cos(glm::radians(25.0f))));
    CHECK(lights[0].cosOuter == doctest::Approx(std::cos(glm::radians(40.0f))));
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
    CHECK_FALSE(scene.system<RenderSystem>()->takeReport(RenderReport::LightLimit));
}

TEST_CASE("Frame block: lights by kind, a spot cone is 1 inside and 0 outside")
{
    FrameData frame;
    frame.lights.push_back({.kind = LightKind::Directional, .color = Vec3(1.0f)});
    frame.lights.push_back({.kind = LightKind::Point, .range = 5.0f});
    LightData spot{.kind = LightKind::Spot, .range = 5.0f};
    spot.cosInner = std::cos(glm::radians(20.0f));
    spot.cosOuter = std::cos(glm::radians(30.0f));
    frame.lights.push_back(spot);

    const FrameUniforms uniforms = frameUniforms(frame);
    CHECK(uniforms.hasSun == 0);
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

TEST_CASE_FIXTURE(Fixture, "New scene: a camera with the sky, the sun and an environment")
{
    editor::buildNewScene(scene, "Scene");
    const RenderSystem& render = *scene.system<RenderSystem>();
    REQUIRE(render.mainCamera());
    CHECK(render.mainCamera()->clear() == CameraClear::Sky);
    REQUIRE(render.environment());
    CHECK(render.environment()->sky.id == builtin::kDefaultSkyMaterial);

    const FrameData frame = extractFrame(scene, kAspect);
    REQUIRE_FALSE(frame.lights.empty());
    CHECK(frame.lights[0].sun);
    CHECK(frame.environment.sky);
    CHECK(frame.environment.atmosphere.enabled);
}

TEST_CASE_FIXTURE(Fixture, "Extract: a scene without its sky and sun takes them from the lighting scene")
{
    Scene lighting{services};
    editor::addDefaultLighting(lighting.createRoot<Node3D>("Lighting"));
    lighting.updateTransforms();

    // Префаб с лампой: своего неба и солнца нет.
    auto& root = scene.createRoot<Node3D>("Lamp");
    root.addChild<PointLight3D>("Bulb");
    FrameData frame = extractFrame(scene, kAspect, &lighting);
    REQUIRE(frame.lights.size() == 2);
    CHECK(frame.lights[0].kind == LightKind::Directional);
    CHECK(frame.lights[0].sun);
    CHECK(frame.lights[1].kind == LightKind::Point);
    REQUIRE(frame.environment.sky);
    CHECK(frame.materials[*frame.environment.sky].id == builtin::kDefaultSkyMaterial);
    // Без сцены света - только своё.
    CHECK(extractFrame(scene, kAspect).lights.size() == 1);
    CHECK_FALSE(extractFrame(scene, kAspect).environment.sky);

    // Своё солнце и своё окружение важнее запасных.
    root.addChild<DirectionalLight3D>("OwnSun").setIntensity(1.0f);
    auto& own = root.addChild<Environment>("Own");
    own.atmosphere.mieAnisotropy = 0.25f;
    frame = extractFrame(scene, kAspect, &lighting);
    CHECK(
        std::ranges::count_if(
            frame.lights,
            [](const LightData& light) {
                return light.kind == LightKind::Directional;
            }
        )
        == 1
    );
    CHECK(frame.environment.atmosphere.mieAnisotropy == 0.25f);
}
