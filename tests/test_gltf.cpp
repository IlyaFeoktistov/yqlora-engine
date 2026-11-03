#include "EngineRegistry.hpp"
#include "GltfBuilder.hpp"

#include <doctest/doctest.h>

#include <yqlora/core/EngineServices.hpp>
#include <yqlora/core/Utf8Path.hpp>
#include <yqlora/import/GltfLoader.hpp>
#include <yqlora/import/ModelData.hpp>
#include <yqlora/input/Input.hpp>
#include <yqlora/project/Project.hpp>
#include <yqlora/render/MeshData.hpp>
#include <yqlora/resource/ResourceCache.hpp>

#include <glm/geometric.hpp>
#include <nlohmann/json.hpp>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace yqlora;
using namespace yqlora::test;
namespace fs = std::filesystem;
using Json = nlohmann::json;

namespace {

fs::path freshDirectory(const char* name)
{
    const auto directory = fs::temp_directory_path() / "yqlora_tests" / name;
    fs::remove_all(directory);
    fs::create_directories(directory);

    return directory;
}

LoadResult load(const fs::path& file, Json import = Json::object())
{
    return GltfLoader{}.load({.file = file, .import = std::move(import)});
}

template <typename T>
const T& data(const LoadedResource& resource)
{
    REQUIRE(resource.kind == T::kResourceKind);

    return *static_cast<const T*>(resource.data.get());
}

// Один треугольник в плоскости XY без нормалей и без индексов.
GltfBuilder plainTriangle()
{
    GltfBuilder gltf;
    const int positions = gltf.positions({0, 0, 0, 1, 0, 0, 0, 1, 0});
    gltf.json["meshes"] = {{{"name", "Tri"}, {"primitives", {{{"attributes", {{"POSITION", positions}}}}}}}};
    gltf.json["nodes"] = {{{"name", "Root"}, {"mesh", 0}, {"translation", {1.0, 2.0, 3.0}}}};

    return gltf;
}

} // namespace

TEST_CASE("GltfLoader: embedded buffer, no indices, no normals: normals are computed, indices go in order")
{
    const fs::path file = freshDirectory("gltf_plain") / "tri.gltf";
    plainTriangle().writeGltf(file);

    const LoadResult result = load(file);
    REQUIRE(result.error.empty());
    REQUIRE(result.resources.size() == 2);
    CHECK(result.resources[0].key.empty());
    const auto& model = data<ModelData>(result.resources[0]);
    REQUIRE(model.nodes.size() == 1);
    CHECK(model.roots == std::vector<std::size_t>{0});
    CHECK(model.nodes[0].name == "Root");
    CHECK(model.nodes[0].position == Vec3(1.0f, 2.0f, 3.0f));
    CHECK(model.nodes[0].mesh == 0);
    CHECK(model.meshes == std::vector<std::string>{"mesh:Tri"});

    CHECK(result.resources[1].key == "mesh:Tri");
    const auto& mesh = data<MeshData>(result.resources[1]);
    CHECK(mesh.indices == std::vector<std::uint32_t>{0, 1, 2});
    for (const Vertex& vertex : mesh.vertices) {
        CHECK(vertex.normal == Vec3(0.0f, 0.0f, 1.0f));
        CHECK(vertex.uv0 == Vec2(0.0f));
    }
    REQUIRE(mesh.submeshes.size() == 1);
    CHECK_FALSE(mesh.submeshes[0].sourceMaterial);
    CHECK(mesh.bounds.max == Vec3(1.0f, 1.0f, 0.0f));
}

TEST_CASE("GltfLoader: GLB with interleaved data, shared and same-named meshes, materials and a skipped primitive")
{
    GltfBuilder gltf;
    // Положение и нормаль вперемежку, шаг 24 байта.
    const std::vector<float> interleaved{
        0, 0, 0, 0, 0, 1, //
        2, 0, 0, 0, 0, 1, //
        0, 2, 0, 0, 0, 1, //
        2, 2, 0, 0, 0, 1,
    };
    const int vertexView = gltf.view(interleaved, 24);
    const Json bounds{{"min", {0.0, 0.0, 0.0}}, {"max", {2.0, 2.0, 0.0}}};
    const int positions = gltf.accessor(vertexView, 0, kFloat, 4, "VEC3", bounds);
    const int normals = gltf.accessor(vertexView, 12, kFloat, 4, "VEC3");
    const int indices = gltf.accessor(
        gltf.view(std::vector<std::uint16_t>{0, 1, 2, 1, 3, 2}),
        0,
        kUnsignedShort,
        6,
        "SCALAR"
    );
    const Json attributes{{"POSITION", positions}, {"NORMAL", normals}};
    gltf.json["materials"] = {{{"name", "Wood"}}, {{"name", "Metal"}}};
    gltf.json["meshes"] = {
        {{"name", "Door"},
         {"primitives",
          {{{"attributes", attributes}, {"indices", indices}, {"material", 1}, {"mode", kTriangles}},
           {{"attributes", attributes}, {"mode", kPoints}},
           {{"attributes", attributes}, {"indices", indices}}}}},
        {{"name", "Door"}, {"primitives", {{{"attributes", attributes}, {"indices", indices}}}}},
        {{"primitives", {{{"attributes", attributes}, {"indices", indices}, {"material", 0}}}}},
    };
    // Две двери с одним мешем; у рамы матрица вместо TRS.
    gltf.json["nodes"] = {
        {{"name", "House"}, {"children", {1, 2, 3}}},
        {{"name", "LeftDoor"}, {"mesh", 0}, {"translation", {-1.0, 0.0, 0.0}}},
        {{"name", "RightDoor"}, {"mesh", 0}, {"translation", {1.0, 0.0, 0.0}}, {"rotation", {0.0, 1.0, 0.0, 0.0}}},
        {{"name", "Frame"},
         {"mesh", 2},
         {"matrix", {2.0, 0.0, 0.0, 0.0, 0.0, 2.0, 0.0, 0.0, 0.0, 0.0, 2.0, 0.0, 5.0, 6.0, 7.0, 1.0}}},
    };
    gltf.json["scenes"] = {{{"nodes", {0}}}};
    gltf.json["scene"] = 0;
    const fs::path file = freshDirectory("gltf_glb") / "house.glb";
    gltf.writeGlb(file);

    const LoadResult result = load(file);
    REQUIRE(result.error.empty());
    REQUIRE(result.resources.size() == 4);
    const auto& model = data<ModelData>(result.resources[0]);
    CHECK(model.meshes == std::vector<std::string>{"mesh:Door", "mesh:Door#1", "mesh:#2"});
    CHECK(model.roots == std::vector<std::size_t>{0});
    CHECK(model.nodes[0].children == std::vector<std::size_t>{1, 2, 3});
    CHECK(model.nodes[1].mesh == model.nodes[2].mesh);
    CHECK(model.nodes[2].rotation.y == doctest::Approx(1.0f));
    CHECK(model.nodes[2].rotation.w == doctest::Approx(0.0f));
    CHECK(model.nodes[3].position == Vec3(5.0f, 6.0f, 7.0f));
    CHECK(model.nodes[3].scale.x == doctest::Approx(2.0f));
    CHECK_FALSE(model.nodes[0].mesh);

    // Точки пропущены, два треугольных примитива - два подмеша.
    const auto& door = data<MeshData>(result.resources[1]);
    CHECK(door.vertices.size() == 8);
    CHECK(door.vertices[3].position == Vec3(2.0f, 2.0f, 0.0f));
    CHECK(door.vertices[3].normal == Vec3(0.0f, 0.0f, 1.0f));
    REQUIRE(door.submeshes.size() == 2);
    CHECK(door.submeshes[0].sourceMaterial == 1);
    CHECK_FALSE(door.submeshes[1].sourceMaterial);
    CHECK(door.submeshes[1].firstIndex == 6);
    CHECK(door.indices[6] == 4);
    CHECK(data<MeshData>(result.resources[3]).submeshes[0].sourceMaterial == 0);
}

TEST_CASE("GltfLoader: an external buffer next to the file, in a folder with a non-ASCII name")
{
    const fs::path directory = freshDirectory("gltf_external") / fromUtf8("модели");
    fs::create_directories(directory);
    plainTriangle().writeGltfWithBin(directory / "tri.gltf", "tri.bin");

    const LoadResult result = load(directory / "tri.gltf");
    REQUIRE(result.error.empty());
    CHECK(data<MeshData>(result.resources[1]).vertices[1].position == Vec3(1.0f, 0.0f, 0.0f));
}

TEST_CASE("GltfLoader: an external buffer is a dependency, and when it is missing too")
{
    const fs::path directory = freshDirectory("gltf_dependency") / fromUtf8("модели");
    fs::create_directories(directory);
    plainTriangle().writeGltfWithBin(directory / "tri.gltf", "tri%20data.bin");
    // URI в glTF закодирован: файл рядом называется с пробелом.
    fs::rename(directory / "tri%20data.bin", directory / "tri data.bin");

    const LoadResult loaded = load(directory / "tri.gltf");
    REQUIRE(loaded.error.empty());
    REQUIRE(loaded.dependencies.size() == 1);
    CHECK(loaded.dependencies[0] == directory / "tri data.bin");

    fs::remove(directory / "tri data.bin");
    const LoadResult missing = load(directory / "tri.gltf");
    CHECK_FALSE(missing.error.empty());
    REQUIRE(missing.dependencies.size() == 1);
    CHECK(missing.dependencies[0] == directory / "tri data.bin");

    // Встроенный буфер (data:) - не файл.
    plainTriangle().writeGltf(directory / "inline.gltf");
    CHECK(load(directory / "inline.gltf").dependencies.empty());
}

TEST_CASE("GltfLoader: sparse positions and normalized integer texture coordinates")
{
    GltfBuilder gltf;
    const int base = gltf.view(std::vector<float>{0, 0, 0, 1, 0, 0, 0, 1, 0});
    // Вершина 2 подменяется разреженным аксессором.
    const int sparseIndices = gltf.view(std::vector<std::uint16_t>{2, 0});
    const int sparseValues = gltf.view(std::vector<float>{0, 4, 0});
    const Json sparse{
        {"count", 1},
        {"indices", {{"bufferView", sparseIndices}, {"componentType", kUnsignedShort}}},
        {"values", {{"bufferView", sparseValues}}}
    };
    const int positions = gltf.accessor(
        base,
        0,
        kFloat,
        3,
        "VEC3",
        {{"sparse", sparse}, {"min", {0.0, 0.0, 0.0}}, {"max", {1.0, 4.0, 0.0}}}
    );
    const int uvs = gltf.accessor(
        gltf.view(std::vector<std::uint16_t>{0, 0, 65535, 0, 0, 65535}),
        0,
        kUnsignedShort,
        3,
        "VEC2",
        {{"normalized", true}}
    );
    gltf.json["meshes"] = {{{"primitives", {{{"attributes", {{"POSITION", positions}, {"TEXCOORD_0", uvs}}}}}}}};
    const fs::path file = freshDirectory("gltf_sparse") / "sparse.gltf";
    gltf.writeGltf(file);

    const LoadResult result = load(file);
    REQUIRE(result.error.empty());
    const auto& mesh = data<MeshData>(result.resources[1]);
    CHECK(mesh.vertices[2].position == Vec3(0.0f, 4.0f, 0.0f));
    CHECK(mesh.vertices[1].uv0 == Vec2(1.0f, 0.0f));
    CHECK(mesh.vertices[2].uv0 == Vec2(0.0f, 1.0f));
    // Без узлов и сцен модель пустая, но меш есть.
    CHECK(data<ModelData>(result.resources[0]).nodes.empty());
}

TEST_CASE("GltfLoader: the scale import setting scales vertices and node positions")
{
    const fs::path file = freshDirectory("gltf_scale") / "tri.gltf";
    plainTriangle().writeGltf(file);

    const LoadResult result = load(file, {{"scale", 0.5}});
    REQUIRE(result.error.empty());
    CHECK(data<ModelData>(result.resources[0]).nodes[0].position == Vec3(0.5f, 1.0f, 1.5f));
    const auto& mesh = data<MeshData>(result.resources[1]);
    CHECK(mesh.vertices[1].position == Vec3(0.5f, 0.0f, 0.0f));
    CHECK(mesh.bounds.max == Vec3(0.5f, 0.5f, 0.0f));
}

TEST_CASE("GltfLoader: broken files give an error, not a crash")
{
    const fs::path directory = freshDirectory("gltf_broken");
    std::ofstream(directory / "text.gltf") << "not json";
    CHECK_FALSE(load(directory / "text.gltf").error.empty());

    std::ofstream(directory / "empty.glb");
    CHECK_FALSE(load(directory / "empty.glb").error.empty());
    CHECK_FALSE(load(directory / "missing.gltf").error.empty());

    // Обрезанный GLB.
    plainTriangle().writeGlb(directory / "whole.glb");
    std::ifstream whole(directory / "whole.glb", std::ios::binary);
    std::string bytes{std::istreambuf_iterator<char>(whole), std::istreambuf_iterator<char>()};
    std::ofstream(directory / "cut.glb", std::ios::binary) << bytes.substr(0, bytes.size() / 2);
    CHECK_FALSE(load(directory / "cut.glb").error.empty());

    // Индекс за пределами вершин.
    GltfBuilder gltf;
    const int positions = gltf.positions({0, 0, 0, 1, 0, 0, 0, 1, 0});
    const int indices = gltf.accessor(gltf.view(std::vector<std::uint16_t>{0, 1, 7}), 0, kUnsignedShort, 3, "SCALAR");
    gltf.json["meshes"] = {{{"primitives", {{{"attributes", {{"POSITION", positions}}}, {"indices", indices}}}}}};
    gltf.writeGltf(directory / "index.gltf");
    CHECK_FALSE(load(directory / "index.gltf").error.empty());
}

TEST_CASE("ResourceCache: meshes of a glTF file are nested resources with ids in .meta")
{
    const fs::path directory = freshDirectory("gltf_cache");
    plainTriangle().writeGlb(directory / "tri.glb");
    test::EngineRegistry registry;
    REQUIRE(Project::create(directory, "Game", registry));
    auto project = Project::open(directory, registry);
    REQUIRE(project);
    Input input;
    EngineServices services{input, registry};
    services.resources().setMetaPolicy(MetaPolicy::Import);
    services.setProject(&*project);

    const ResourceId file = project->resources().id("tri.glb");
    const ModelData* model = services.resources().find<ModelData>(file);
    REQUIRE(model);
    const ResourceId mesh = project->resources().subresources(file).at(model->meshes[0]);
    REQUIRE(services.resources().find<MeshData>(mesh));
    CHECK(services.resources().find<MeshData>(mesh)->vertices.size() == 3);
    // Модель - не меш.
    CHECK(services.resources().find<MeshData>(file) == nullptr);
    services.setProject(nullptr);
}
