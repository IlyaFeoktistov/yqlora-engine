#include "EngineRegistry.hpp"
#include <doctest/doctest.h>

#include <yqlora/core/EngineServices.hpp>

#include <yqlora/input/Input.hpp>

#include <yqlora/core/EngineTypes.hpp>
#include <yqlora/reflection/TypeRegistry.hpp>
#include <yqlora/render/BuiltinMeshes.hpp>
#include <yqlora/render/Camera3D.hpp>
#include <yqlora/render/FrameBuilder.hpp>
#include <yqlora/render/MeshData.hpp>
#include <yqlora/render/MeshRenderer.hpp>
#include <yqlora/resource/BuiltinResources.hpp>
#include <yqlora/resource/ResourceCache.hpp>
#include <yqlora/scene/Scene.hpp>

#include <algorithm>
#include <vector>
#include <yqlora/scene/SceneSerializer.hpp>

#include <glm/geometric.hpp>
#include <glm/vector_relational.hpp>

using namespace yqlora;

TEST_CASE("MeshData: builtin meshes are valid and face outwards")
{
    ResourceCache cache;
    for (const BuiltinMesh& builtinMesh : builtinMeshes()) {
        CAPTURE(builtinMesh.name);
        const MeshData* mesh = cache.find<MeshData>(builtinMesh.id);
        REQUIRE(mesh);
        REQUIRE(mesh->indices.size() % 3 == 0);

        for (const std::uint32_t index : mesh->indices) {
            REQUIRE(index < mesh->vertices.size());
        }
        for (const Vertex& vertex : mesh->vertices) {
            CHECK(glm::length(vertex.normal) == doctest::Approx(1.0f));
            CHECK(vertex.uv0.x >= 0.0f);
            CHECK(vertex.uv0.x <= 1.0f);
            CHECK(vertex.uv0.y >= 0.0f);
            CHECK(vertex.uv0.y <= 1.0f);
            CHECK(glm::all(glm::greaterThanEqual(vertex.position, mesh->bounds.min)));
            CHECK(glm::all(glm::lessThanEqual(vertex.position, mesh->bounds.max)));
        }
        // Один материал на весь меш.
        REQUIRE(mesh->submeshes.size() == 1);
        CHECK(mesh->submeshes[0].firstIndex == 0);
        CHECK(mesh->submeshes[0].indexCount == mesh->indices.size());
        CHECK_FALSE(mesh->submeshes[0].sourceMaterial);

        // Против часовой стрелки снаружи: нормаль треугольника по порядку вершин
        // смотрит туда же, куда нормали вершин. Иначе грань отсечётся как задняя.
        for (std::size_t i = 0; i < mesh->indices.size(); i += 3) {
            const Vertex& a = mesh->vertices[mesh->indices[i]];
            const Vertex& b = mesh->vertices[mesh->indices[i + 1]];
            const Vertex& c = mesh->vertices[mesh->indices[i + 2]];
            const Vec3 faceNormal = glm::cross(b.position - a.position, c.position - a.position);
            // У полюсов сферы треугольники вырождаются в линию, их пропускаем.
            if (glm::length(faceNormal) < 1e-6f) {
                continue;
            }
            CHECK(glm::dot(faceNormal, a.normal + b.normal + c.normal) > 0.0f);
        }
    }

    CHECK(cache.find<MeshData>(0) == nullptr);
    CHECK(cache.find<MeshData>(builtin::kFirstProjectId) == nullptr);
}

TEST_CASE("FrameBuilder: every Node3D with a MeshRenderer and a known mesh becomes a draw item")
{
    Scene scene{EngineServices::none()};
    auto& root = scene.createRoot<Node3D>("Root");
    root.transform().setPosition({0.0f, 1.0f, 0.0f});

    auto& cube = root.addChild<Node3D>("Cube");
    cube.transform().setPosition({2.0f, 0.0f, 0.0f});
    auto& cubeRenderer = cube.addComponent<MeshRenderer>();
    cubeRenderer.setMesh(ResourceRef{builtin::kCubeMesh});

    // Без меша ничего не рисуется.
    root.addChild<Node3D>("Empty").addComponent<MeshRenderer>();

    auto& unknown = root.addChild<Node3D>("Unknown");
    unknown.addComponent<MeshRenderer>().setMesh(ResourceRef{123456});

    const FrameData frame = extractFrame(scene, 1.0f);
    REQUIRE(frame.drawItems.size() == 1);
    const DrawItem& item = frame.drawItems[0];
    CHECK(item.mesh == scene.services().resources().find<MeshData>(builtin::kCubeMesh));
    CHECK(item.meshId == builtin::kCubeMesh);
    CHECK(item.meshRevision == scene.services().resources().status(builtin::kCubeMesh)->revision);
    CHECK(item.node == cube.id());
    // Весь куб - один подмеш; пустой слот рисуется материалом по умолчанию.
    CHECK(item.firstIndex == 0);
    CHECK(item.indexCount == item.mesh->indices.size());
    REQUIRE(item.material < frame.materials.size());
    CHECK(frame.materials[item.material].id == builtin::kDefaultMaterial);
    // Мировая матрица учитывает родителя.
    CHECK(Vec3(item.world[3]) == Vec3(2.0f, 1.0f, 0.0f));
}

TEST_CASE("MeshRenderer: mesh and materials are saved with the scene, overrides are not")
{
    TypeRegistry registry;
    registerEngineTypes(registry);
    const SceneSerializer serializer(registry);

    Scene original{EngineServices::none()};
    auto& ball = original.createRoot<Node3D>("Ball");
    auto& renderer = ball.addComponent<MeshRenderer>();
    renderer.setMesh(ResourceRef{builtin::kSphereMesh});
    renderer.setMaterials({ResourceRef{builtin::kErrorMaterial}, ResourceRef{}});
    renderer.overrides(0).setFloat("roughness", 0.1f);

    Scene loaded{EngineServices::none()};
    REQUIRE(serializer.fromJson(loaded, serializer.toJson(original)));
    const auto* loadedRenderer = loaded.root()->getComponent<MeshRenderer>();
    REQUIRE(loadedRenderer);
    CHECK(loadedRenderer->mesh().id == builtin::kSphereMesh);
    CHECK(loadedRenderer->materials() == std::vector<ResourceRef>{{builtin::kErrorMaterial}, {}});
    CHECK(loadedRenderer->findOverrides(0) == nullptr);
}

TEST_CASE("SceneSerializer: a scene from a newer engine is not opened")
{
    TypeRegistry registry;
    registerEngineTypes(registry);
    const SceneSerializer serializer(registry);

    Scene scene{EngineServices::none()};
    CHECK_FALSE(serializer.fromJson(scene, R"({"version": 99, "root": null})"));
    CHECK_FALSE(serializer.fromJson(scene, R"({"root": null})"));
}

TEST_CASE("MeshRenderer: a scene with the old color opens with empty slots and keeps it as unknown")
{
    TypeRegistry registry;
    registerEngineTypes(registry);
    const SceneSerializer serializer(registry);
    Scene original{EngineServices::none()};
    original.createRoot<Node3D>("Ball").addComponent<MeshRenderer>();
    auto json = nlohmann::json::parse(serializer.toJson(original));
    const auto rendererJson = [](nlohmann::json& scene) -> nlohmann::json& {
        for (auto& component : scene["root"]["components"]) {
            if (component["type"] == "MeshRenderer") {
                return component;
            }
        }
        FAIL("no MeshRenderer in the scene file");

        return scene;
    };
    rendererJson(json)["properties"]["color"] = {1.0, 0.0, 0.0, 1.0};

    Scene loaded{EngineServices::none()};
    REQUIRE(serializer.fromJson(loaded, json.dump()));
    const auto* renderer = loaded.root()->getComponent<MeshRenderer>();
    REQUIRE(renderer);
    CHECK(renderer->materials().empty());
    auto saved = nlohmann::json::parse(serializer.toJson(loaded));
    CHECK(rendererJson(saved)["properties"].contains("color"));
}

TEST_CASE("MeshRenderer: a slot per submesh of its mesh, the materials property is labelled by them")
{
    test::EngineRegistry registry;
    Input input;
    EngineServices services{input, registry};
    Scene scene{services};
    auto& renderer = scene.createRoot<Node3D>("Cube").addComponent<MeshRenderer>();
    CHECK(renderer.slotNames().empty());
    renderer.setMesh({builtin::kCubeMesh});
    CHECK(renderer.slotNames() == std::vector<std::string>{""});

    const TypeInfo* type = registry.find("MeshRenderer");
    REQUIRE(type);
    const auto properties = registry.properties(*type);
    const auto materials = std::ranges::find(properties, std::string("materials"), &Property::name);
    REQUIRE(materials != properties.end());
    REQUIRE((*materials)->elementLabels());
    CHECK((*materials)->elementLabels()(renderer).size() == 1);
}
