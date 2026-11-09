#include "EngineRegistry.hpp"

#include <doctest/doctest.h>

#include <yqlora/input/Input.hpp>
#include <yqlora/reflection/TypeRegistry.hpp>
#include <yqlora/render/BuiltinMeshes.hpp>
#include <yqlora/render/Camera3D.hpp>
#include <yqlora/render/FrameBuilder.hpp>
#include <yqlora/render/MeshRenderer.hpp>
#include <yqlora/render/RenderSystem.hpp>
#include <yqlora/resource/BuiltinResources.hpp>
#include <yqlora/scene/Scene.hpp>

using namespace yqlora;

TEST_CASE("Camera3D: view matrix moves the world opposite to the camera")
{
    Camera3D camera;
    camera.transform().setPosition({0.0f, 0.0f, 5.0f});

    // Начало координат оказывается в 5 единицах перед камерой (по -Z).
    const Vec4 origin = camera.viewMatrix() * Vec4(0.0f, 0.0f, 0.0f, 1.0f);
    CHECK(origin.z == doctest::Approx(-5.0f));
}

TEST_CASE("Camera3D: clip depth goes from 0 at the near plane to 1 at the far plane")
{
    Camera3D camera;
    camera.setClipPlanes(1.0f, 100.0f);

    auto ndcDepth = [&](float viewZ) {
        const Vec4 clip = camera.projectionMatrix(1.0f) * Vec4(0.0f, 0.0f, viewZ, 1.0f);

        return clip.z / clip.w;
    };

    CHECK(ndcDepth(-1.0f) == doctest::Approx(0.0f));
    CHECK(ndcDepth(-100.0f) == doctest::Approx(1.0f));
}

namespace {

struct Fixture {
    test::EngineRegistry registry;
    Input input;
    EngineServices services{input, registry};
    Scene scene{services};
};

} // namespace

TEST_CASE_FIXTURE(Fixture, "RenderSystem: active cameras by order, the first one is the main")
{
    auto& root = scene.createRoot<Node3D>();
    auto& arms = root.addChild<Camera3D>("Arms");
    arms.setOrder(1);
    auto& world = root.addChild<Camera3D>("World");
    auto& spare = root.addChild<Camera3D>("Spare");
    spare.setOrder(-1);
    spare.setActive(false);

    const RenderSystem& render = *scene.system<RenderSystem>();
    CHECK(render.cameras() == std::vector<Camera3D*>{&world, &arms});
    CHECK(render.mainCamera() == &world);

    // Ушедшая из сцены камера выпадает из списка сама.
    auto removed = root.removeChild(world);
    CHECK(render.mainCamera() == &arms);
    scene.setRoot(nullptr);
    CHECK(render.mainCamera() == nullptr);
}

TEST_CASE_FIXTURE(Fixture, "RenderSystem: order to become main is one below the main camera")
{
    auto& root = scene.createRoot<Node3D>();
    auto& world = root.addChild<Camera3D>("World");
    auto& arms = root.addChild<Camera3D>("Arms");
    const RenderSystem& render = *scene.system<RenderSystem>();

    // При равном order главная - кто раньше вошёл, поэтому нужен строго меньший.
    CHECK(orderToBecomeMain(render, arms) == -1);
    CHECK(orderToBecomeMain(render, world) == std::nullopt);

    arms.setActive(false);
    CHECK(orderToBecomeMain(render, arms) == std::nullopt);
}

TEST_CASE_FIXTURE(Fixture, "Extract: one view per camera with its clear, background and mask")
{
    auto& root = scene.createRoot<Node3D>();
    auto& world = root.addChild<Camera3D>("World");
    world.transform().setPosition({1.0f, 2.0f, 3.0f});
    world.setBackground({0.1f, 0.2f, 0.3f, 1.0f});
    auto& arms = root.addChild<Camera3D>("Arms");
    arms.setOrder(1);
    arms.setClear(CameraClear::DepthOnly);
    arms.setCullingMask(LayerMask{1} << 5);

    auto& weapon = root.addChild<Node3D>("Weapon");
    weapon.setLayer(5);
    weapon.addComponent<MeshRenderer>().setMesh(ResourceRef{builtin::kCubeMesh});

    const FrameData frame = extractFrame(scene, 16.0f / 9.0f);
    REQUIRE(frame.views.size() == 2);
    CHECK(frame.views[0].camera.position == Vec3(1.0f, 2.0f, 3.0f));
    // Новая камера рисует за объектами небо.
    CHECK(frame.views[0].clear == CameraClear::Sky);
    // Камера хранит фон в sRGB, кадр рисуется в линейном.
    CHECK(math::nearlyEqual(frame.views[0].background, math::srgbToLinear(Vec4(0.1f, 0.2f, 0.3f, 1.0f))));
    CHECK(frame.views[1].clear == CameraClear::DepthOnly);
    REQUIRE(frame.drawItems.size() == 1);
    CHECK(frame.drawItems[0].layer == 5);
    CHECK(inMask(frame.views[0].mask, 5));
    CHECK(inMask(frame.views[1].mask, 5));
    CHECK_FALSE(inMask(frame.views[1].mask, 0));
}

TEST_CASE_FIXTURE(Fixture, "Camera3D: exposure in EV100 and tone mapping go into the view")
{
    // ISO 100: 1 / (1.2 * 2^EV100); EV100 15 - солнечный день.
    CHECK(exposureFactor(15.0f) == doctest::Approx(1.0f / (1.2f * 32768.0f)));
    CHECK(exposureFactor(0.0f) == doctest::Approx(1.0f / 1.2f));
    CHECK(exposureFactor(16.0f) == doctest::Approx(exposureFactor(15.0f) / 2.0f));

    auto& root = scene.createRoot<Node3D>();
    auto& camera = root.addChild<Camera3D>("Camera");
    CHECK(camera.exposure() == kDefaultCameraExposure);
    CHECK(camera.toneMapping() == ToneMapping::AgX);
    camera.setExposure(12.0f);
    camera.setToneMapping(ToneMapping::Neutral);

    const FrameData frame = extractFrame(scene, 1.0f);
    REQUIRE(frame.views.size() == 1);
    CHECK(frame.views[0].exposure.factor == doctest::Approx(exposureFactor(12.0f)));
    CHECK(frame.views[0].exposure.toneMapping == ToneMapping::Neutral);
    // Без камеры - как у новой камеры.
    CHECK(viewExposure(nullptr).factor == doctest::Approx(exposureFactor(kDefaultCameraExposure)));
    CHECK(viewExposure(nullptr).toneMapping == kDefaultCameraToneMapping);
    // Вид, собранный вручную, нейтрален.
    CHECK(ViewData{}.exposure.factor == 1.0f);
    CHECK(ViewData{}.exposure.toneMapping == ToneMapping::None);
}

TEST_CASE("Node: layer stays inside the layer range")
{
    Node node;
    node.setLayer(kLayerCount);
    CHECK(node.layer() == kLayerCount - 1);
    node.setLayer(-1);
    CHECK(node.layer() == 0);
}
