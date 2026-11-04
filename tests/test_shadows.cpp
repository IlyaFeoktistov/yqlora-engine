#include "EngineRegistry.hpp"

#include <doctest/doctest.h>

#include <yqlora/core/EngineServices.hpp>
#include <yqlora/input/Input.hpp>
#include <yqlora/render/Camera3D.hpp>
#include <yqlora/render/DirectionalLight3D.hpp>
#include <yqlora/render/EngineUniforms.hpp>
#include <yqlora/render/FrameBuilder.hpp>
#include <yqlora/render/FrameExtract.hpp>
#include <yqlora/render/GraphicsSettings.hpp>
#include <yqlora/render/MeshRenderer.hpp>
#include <yqlora/render/PointLight3D.hpp>
#include <yqlora/render/RenderSystem.hpp>
#include <yqlora/render/ShadowCascades.hpp>
#include <yqlora/render/SpotLight3D.hpp>
#include <yqlora/resource/BuiltinResources.hpp>
#include <yqlora/scene/Scene.hpp>

#include <cmath>
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
constexpr float kNear = 0.1f;
constexpr float kFar = 500.0f;

CameraData cameraAt(const Vec3& position, const Vec3& target)
{
    return {
        .view = glm::lookAt(position, target, Vec3(0.0f, 1.0f, 0.0f)),
        .projection = math::perspective(glm::radians(60.0f), kAspect, kNear, kFar),
        .position = position,
    };
}

// Тексель карты каскада, в который попадает точка мира: дробная часть -
// где точка внутри текселя.
Vec2 texelOf(const ShadowCascade& cascade, int mapSize, const Vec3& point)
{
    const Vec4 clip = cascade.viewProjection * Vec4(point, 1.0f);

    return (Vec2(clip) / clip.w * 0.5f + 0.5f) * static_cast<float>(mapSize);
}

} // namespace

TEST_CASE("Shadow cascades: splits grow and the last one is the shadow distance")
{
    constexpr float kDistance = 100.0f;
    const std::vector<float> splits = cascadeSplits(kNear, kDistance, kMaxShadowCascades);
    REQUIRE(splits.size() == static_cast<std::size_t>(kMaxShadowCascades));
    float previous = kNear;
    float previousWidth = 0.0f;
    for (const float split : splits) {
        CHECK(split > previous);
        // Каждый следующий каскад шире: ближние мельче и резче.
        CHECK(split - previous > previousWidth);
        previousWidth = split - previous;
        previous = split;
    }
    CHECK(splits.back() == doctest::Approx(kDistance));
    CHECK(cascadeSplits(kNear, kDistance, 1) == std::vector<float>{kDistance});
}

TEST_CASE("Shadow cascades: they cover the distance, keep their size when the camera turns, follow texels")
{
    const ShadowSettings settings{};
    const Vec3 toSun = glm::normalize(Vec3(0.3f, 1.0f, 0.2f));
    const CameraData camera = cameraAt(Vec3(0.0f, 2.0f, 0.0f), Vec3(0.0f, 2.0f, -1.0f));
    const auto cascades = fitShadowCascades(camera, toSun, settings, {});
    REQUIRE(cascades.size() == static_cast<std::size_t>(settings.cascades));
    CHECK(cascades.back().splitFar == doctest::Approx(settings.distance));

    // Точка в конце дальности попадает в последний каскад.
    const Vec4 far = cascades.back().viewProjection * Vec4(0.0f, 2.0f, -settings.distance * 0.99f, 1.0f);
    CHECK(std::abs(far.x / far.w) <= 1.0f);
    CHECK(std::abs(far.y / far.w) <= 1.0f);

    // Поворот камеры не меняет размер каскада: тексель тот же.
    const CameraData turned = cameraAt(Vec3(0.0f, 2.0f, 0.0f), Vec3(1.0f, 2.0f, -0.3f));
    const auto turnedCascades = fitShadowCascades(turned, toSun, settings, {});
    for (std::size_t i = 0; i < cascades.size(); ++i) {
        CHECK(turnedCascades[i].width == doctest::Approx(cascades[i].width));
    }

    // Сдвиг камеры меньше текселя: точка мира остаётся в том же месте
    // своего текселя, то есть сетка текселей не ползёт и тень не дрожит.
    const float texel = cascades[0].width / static_cast<float>(settings.cascadeSize);
    const Vec3 point(0.4f, 0.0f, -3.0f);
    const CameraData moved = cameraAt(Vec3(texel * 0.3f, 2.0f, 0.0f), Vec3(texel * 0.3f, 2.0f, -1.0f));
    const auto movedCascades = fitShadowCascades(moved, toSun, settings, {});
    const Vec2 before = texelOf(cascades[0], settings.cascadeSize, point);
    const Vec2 after = texelOf(movedCascades[0], settings.cascadeSize, point);
    const Vec2 shift = after - before;
    CHECK(std::abs(shift.x - std::round(shift.x)) < 0.01f);
    CHECK(std::abs(shift.y - std::round(shift.y)) < 0.01f);
}

TEST_CASE("Shadow cascades: the box reaches up to a caster between the cascade and the sun")
{
    const ShadowSettings settings{.cascades = 1};
    const Vec3 toSun(0.0f, 1.0f, 0.0f);
    const CameraData camera = cameraAt(Vec3(0.0f, 2.0f, 0.0f), Vec3(0.0f, 2.0f, -1.0f));
    const float plain = fitShadowCascades(camera, toSun, settings, {}).front().depthRange;

    // Облако высоко над всем, что видит камера, всё равно отбрасывает тень.
    DrawItem cloud{.bounds = {Vec3(-1.0f, 500.0f, -10.0f), Vec3(1.0f, 501.0f, -8.0f)}};
    const float withCloud = fitShadowCascades(camera, toSun, settings, std::span(&cloud, 1)).front().depthRange;
    CHECK(withCloud > plain + 400.0f);
    // Без castShadows - нет.
    cloud.castShadows = false;
    CHECK(
        fitShadowCascades(camera, toSun, settings, std::span(&cloud, 1)).front().depthRange == doctest::Approx(plain)
    );
}

TEST_CASE_FIXTURE(
    Fixture,
    "Extract: shadow tiles go to the nearest lights, a point light takes six, the rest go without"
)
{
    auto& root = scene.createRoot<Node3D>();
    root.addChild<Camera3D>("Camera");
    // Три точечных по 6 плиток и прожектор: в 16 плиток помещаются два
    // ближних точечных и прожектор, дальний точечный - без теней.
    for (const float distance : {3.0f, 1.0f, 2.0f}) {
        auto& point = root.addChild<PointLight3D>("Point");
        point.transform().setPosition(Vec3(0.0f, 0.0f, -distance));
        point.setShadows(ShadowMode::Soft);
    }
    auto& spot = root.addChild<SpotLight3D>("Spot");
    spot.transform().setPosition(Vec3(0.0f, 0.0f, -4.0f));
    spot.setShadows(ShadowMode::Hard);
    // Без теней плиток не берёт.
    root.addChild<PointLight3D>("Plain");
    scene.updateTransforms();

    const FrameData frame = extractFrame(scene, kAspect);
    REQUIRE(frame.shadowTiles.size() == static_cast<std::size_t>(2 * kPointShadowFaces + 1));
    std::vector<float> withShadows;
    for (const LightData& light : frame.lights) {
        if (light.shadows != ShadowMode::None) {
            REQUIRE(light.firstShadowTile >= 0);
            CHECK(
                frame.shadowTiles[static_cast<std::size_t>(light.firstShadowTile)].light
                == static_cast<std::size_t>(&light - frame.lights.data())
            );
            withShadows.push_back(-light.position.z);
        } else {
            CHECK(light.firstShadowTile == -1);
        }
    }
    std::ranges::sort(withShadows);
    CHECK(withShadows == std::vector<float>{1.0f, 2.0f, 4.0f});
}

TEST_CASE_FIXTURE(Fixture, "Extract: shadows off in the settings leave no light with shadows and no tiles")
{
    auto& root = scene.createRoot<Node3D>();
    root.addChild<Camera3D>("Camera");
    root.addChild<DirectionalLight3D>("Sun");
    root.addChild<PointLight3D>("Point").setShadows(ShadowMode::Soft);
    scene.updateTransforms();

    FrameData frame = extractFrame(scene, kAspect);
    CHECK(frame.lights.front().shadows == ShadowMode::Soft);
    CHECK_FALSE(frame.shadowTiles.empty());

    GraphicsSettings settings;
    settings.shadows = false;
    std::vector<LightData> lights = frame.lights;
    frame = FrameData{};
    frame.lights = std::move(lights);
    frame.shadows = shadowSettings(settings);
    for (LightData& light : frame.lights) {
        light.firstShadowTile = -1;
    }
    extractShadowTiles(*scene.system<RenderSystem>(), Vec3(0.0f), frame);
    CHECK(frame.shadowTiles.empty());
    for (const LightData& light : frame.lights) {
        CHECK(light.shadows == ShadowMode::None);
    }
}

TEST_CASE("Shadows: settings stay in their limits, the Shadows block gets the sun and the tiles")
{
    GraphicsSettings settings;
    settings.shadowCascades = 9;
    settings.shadowFilterSamples = 1000;
    settings.shadowDistance = -5.0f;
    const ShadowSettings clamped = shadowSettings(settings);
    CHECK(clamped.cascades == kMaxShadowCascades);
    CHECK(clamped.filterSamples == kMaxShadowSamples);
    CHECK(clamped.distance > 0.0f);

    FrameData frame;
    frame.lights = {
        {.kind = LightKind::Directional, .sun = true, .shadows = ShadowMode::Soft, .sourceSize = 0.01f},
        {.kind = LightKind::Spot, .range = 10.0f, .cosOuter = 0.5f, .shadows = ShadowMode::Hard, .firstShadowTile = 0},
    };
    frame.shadowTiles = {{.viewProjection = shadowTileMatrix(frame.lights[1], 0), .light = 1}};
    const CameraData camera = cameraAt(Vec3(0.0f), Vec3(0.0f, 0.0f, -1.0f));
    const auto cascades = fitShadowCascades(camera, Vec3(0.0f, 1.0f, 0.0f), frame.shadows, {});
    const ShadowUniforms uniforms = shadowUniforms(frame, ViewData{.camera = camera}, cascades);
    CHECK(uniforms.cascadeCount == frame.shadows.cascades);
    CHECK(uniforms.sunShadows == static_cast<int>(ShadowMode::Soft));
    CHECK(uniforms.sunSourceTangent == doctest::Approx(std::tan(0.01f)));
    CHECK(uniforms.punctualShadows[0].x == 0);
    CHECK(uniforms.punctualShadows[0].y == static_cast<int>(ShadowMode::Hard));
    // Масштаб проекции прожектора с внешним углом 60 градусов.
    CHECK(uniforms.punctualShadowSizes[0].y == doctest::Approx(1.0f / std::tan(std::acos(0.5f))));
    CHECK(uniforms.punctualShadowSizes[0].z == doctest::Approx(10.0f));

    // Без каскадов солнце в блоке без теней.
    CHECK(shadowUniforms(frame, ViewData{.camera = camera}, {}).sunShadows == 0);
}
