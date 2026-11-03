#include "EngineRegistry.hpp"

#include <doctest/doctest.h>

#include <yqlora/core/EngineServices.hpp>
#include <yqlora/editor/EditorSettings.hpp>
#include <yqlora/editor/Picking.hpp>
#include <yqlora/editor/Snapping.hpp>
#include <yqlora/editor/ViewportProjection.hpp>
#include <yqlora/input/Input.hpp>
#include <yqlora/render/MeshRenderer.hpp>
#include <yqlora/resource/BuiltinResources.hpp>
#include <yqlora/scene/Node3D.hpp>
#include <yqlora/scene/Scene.hpp>

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <filesystem>
#include <fstream>

using namespace yqlora;
using namespace yqlora::editor;

namespace {

void checkNear(const Vec3& a, const Vec3& b)
{
    CHECK(a.x == doctest::Approx(b.x));
    CHECK(a.y == doctest::Approx(b.y));
    CHECK(a.z == doctest::Approx(b.z));
}

// Пол - куб 10 x 1 x 10 верхней гранью на y = 0, на нём ящик 1 x 1 x 1 с
// центром в (0, 0.5, 0).
struct Fixture {
    Fixture()
    {
        root = &scene.createRoot<Node3D>("Root");
        floor = &root->addChild<Node3D>("Floor");
        floor->transform().setPosition({0.0f, -0.5f, 0.0f});
        floor->transform().setScale({10.0f, 1.0f, 10.0f});
        floor->addComponent<MeshRenderer>().setMesh(ResourceRef{builtin::kCubeMesh});
        box = &root->addChild<Node3D>("Box");
        box->transform().setPosition({0.0f, 0.5f, 0.0f});
        box->addComponent<MeshRenderer>().setMesh(ResourceRef{builtin::kCubeMesh});
        scene.updateTransforms();
    }

    test::EngineRegistry registry;
    Input input;
    EngineServices services{input, registry};
    Scene scene{services};
    Node3D* root = nullptr;
    Node3D* floor = nullptr;
    Node3D* box = nullptr;
};

} // namespace

TEST_CASE("snapToGrid: the nearest point of the grid, a step of zero changes nothing")
{
    checkNear(snapToGrid({1.26f, -0.74f, 3.0f}, 0.5f), Vec3(1.5f, -0.5f, 3.0f));
    checkNear(snapToGrid({1.26f, -0.74f, 3.0f}, 1.0f), Vec3(1.0f, -1.0f, 3.0f));
    checkNear(snapToGrid({1.26f, 2.0f, 3.0f}, 0.0f), Vec3(1.26f, 2.0f, 3.0f));
}

TEST_CASE("alignUpTo: the local up axis turns along the normal by the shortest turn")
{
    const Quat turned = alignUpTo(Quat(1.0f, 0.0f, 0.0f, 0.0f), {1.0f, 0.0f, 0.0f});
    checkNear(turned * Vec3(0.0f, 1.0f, 0.0f), Vec3(1.0f, 0.0f, 0.0f));
    // Ось вдоль нормали: поворот не меняется.
    const Quat yaw = glm::angleAxis(1.0f, Vec3(0.0f, 1.0f, 0.0f));
    const Quat same = alignUpTo(yaw, {0.0f, 2.0f, 0.0f});
    checkNear(same * Vec3(1.0f, 0.0f, 0.0f), yaw * Vec3(1.0f, 0.0f, 0.0f));
}

TEST_CASE_FIXTURE(Fixture, "raycastSurfaces: the nearest hit with its normal, the skipped branch is not hit")
{
    const Ray down{Vec3(0.2f, 5.0f, 0.1f), Vec3(0.0f, -1.0f, 0.0f)};
    const auto onBox = raycastSurfaces(scene, down);
    REQUIRE(onBox);
    CHECK(onBox->node == box->id());
    checkNear(onBox->point, Vec3(0.2f, 1.0f, 0.1f));
    checkNear(onBox->normal, Vec3(0.0f, 1.0f, 0.0f));

    // Ставим сам ящик: он себя не задевает и встаёт на пол.
    const auto onFloor = raycastSurfaces(scene, down, box);
    REQUIRE(onFloor);
    CHECK(onFloor->node == floor->id());
    checkNear(onFloor->point, Vec3(0.2f, 0.0f, 0.1f));
    // У растянутого пола нормаль всё равно единичная и вверх.
    checkNear(onFloor->normal, Vec3(0.0f, 1.0f, 0.0f));

    CHECK_FALSE(raycastSurfaces(scene, {Vec3(0.0f, 5.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f)}));
}

TEST_CASE_FIXTURE(Fixture, "dropPoint: onto a surface, onto the ground past the meshes, to the grid when it is on")
{
    SnapSettings snap;
    checkNear(dropPoint(scene, {Vec3(0.3f, 5.0f, 0.3f), Vec3(0.0f, -1.0f, 0.0f)}, snap), Vec3(0.3f, 1.0f, 0.3f));
    checkNear(dropPoint(scene, {Vec3(20.3f, 5.0f, 0.0f), Vec3(0.0f, -1.0f, 0.0f)}, snap), Vec3(20.3f, 0.0f, 0.0f));
    snap.grid = true;
    snap.moveStep = 0.5f;
    checkNear(dropPoint(scene, {Vec3(20.3f, 5.0f, 0.1f), Vec3(0.0f, -1.0f, 0.0f)}, snap), Vec3(20.5f, 0.0f, 0.0f));
}

TEST_CASE_FIXTURE(Fixture, "nearestVertex: the vertex closest on the screen, only of the nodes the filter takes")
{
    // Камера над сценой смотрит вниз.
    const Mat4 view = glm::lookAt(Vec3(0.0f, 10.0f, 0.0f), Vec3(0.0f), Vec3(0.0f, 0.0f, -1.0f));
    const Mat4 projection = glm::perspective(glm::radians(60.0f), 1.0f, 0.1f, 100.0f);
    const Vec2 size(100.0f, 100.0f);
    const Vec2 center(50.0f, 50.0f);

    const auto onBox = nearestVertex(scene, projection * view, size, center, [&](const Node3D& node) {
        return &node == box;
    });
    REQUIRE(onBox);
    // В перспективе сверху нижние углы ящика на экране ближе к центру.
    CHECK(onBox->world.y == doctest::Approx(0.0f));
    CHECK(std::abs(onBox->world.x) == doctest::Approx(0.5f));
    CHECK(onBox->screenDistance > 0.0f);

    const auto onFloor = nearestVertex(scene, projection * view, size, center, [&](const Node3D& node) {
        return &node == floor;
    });
    REQUIRE(onFloor);
    CHECK(std::abs(onFloor->world.x) == doctest::Approx(5.0f));
    CHECK_FALSE(nearestVertex(scene, projection * view, size, center, [](const Node3D&) {
        return false;
    }));
}

TEST_CASE("EditorSettings: snapping survives a restart, a broken step stays default")
{
    const auto file = std::filesystem::temp_directory_path() / "yqlora_tests" / "snap_editor.json";
    std::filesystem::create_directories(file.parent_path());
    std::filesystem::remove(file);
    EditorSettings settings(file);
    settings.load();
    CHECK(settings.snap == SnapSettings{});
    settings.snap.grid = true;
    settings.snap.moveStep = 0.5f;
    settings.snap.alignToSurface = true;
    REQUIRE(settings.save());

    EditorSettings reloaded(file);
    reloaded.load();
    CHECK(reloaded.snap == settings.snap);

    std::ofstream(file, std::ios::trunc) << R"({"snap": {"moveStep": 0, "rotateStepDegrees": "x"}})";
    EditorSettings broken(file);
    broken.load();
    CHECK(broken.snap.moveStep == SnapSettings::kDefaultMoveStep);
    CHECK(broken.snap.rotateStepDegrees == SnapSettings::kDefaultRotateStepDegrees);
}

TEST_CASE("projectToViewport: the centre of the view is the centre of the window, behind the camera is nothing")
{
    const Vec2 size(200.0f, 100.0f);
    const Mat4 projection = glm::perspectiveRH_ZO(glm::radians(60.0f), size.x / size.y, 0.1f, 100.0f);
    const auto centre = editor::projectToViewport(projection, Vec3(0.0f, 0.0f, -5.0f), size);
    REQUIRE(centre);
    CHECK(centre->x == doctest::Approx(100.0f));
    CHECK(centre->y == doctest::Approx(50.0f));
    // Выше центра в мире - выше на экране, то есть меньше y.
    CHECK(editor::projectToViewport(projection, Vec3(0.0f, 1.0f, -5.0f), size)->y < 50.0f);
    CHECK_FALSE(editor::projectToViewport(projection, Vec3(0.0f, 0.0f, 5.0f), size));
}
