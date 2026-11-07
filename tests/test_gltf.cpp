#include "EngineRegistry.hpp"
#include "GltfBuilder.hpp"
#include "TestFiles.hpp"

#include <doctest/doctest.h>

#include <yqlora/core/EngineServices.hpp>
#include <yqlora/core/Utf8Path.hpp>
#include <yqlora/import/GltfLoader.hpp>
#include <yqlora/import/ModelData.hpp>
#include <yqlora/input/Input.hpp>
#include <yqlora/material/MaterialData.hpp>
#include <yqlora/project/Project.hpp>
#include <yqlora/render/MeshData.hpp>
#include <yqlora/render/TextureData.hpp>
#include <yqlora/resource/BuiltinResources.hpp>
#include <yqlora/resource/ResourceCache.hpp>

#include <glm/geometric.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

using namespace yqlora;
using namespace yqlora::test;
namespace fs = std::filesystem;
using Json = nlohmann::json;

namespace {

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

// Ресурс с ключом key; падает, если его нет.
const LoadedResource& byKey(const LoadResult& result, std::string_view key)
{
    for (const LoadedResource& resource : result.resources) {
        if (resource.key == key) {
            return resource;
        }
    }
    FAIL("no resource " << key);
    throw std::logic_error("unreachable");
}

bool has(const LoadResult& result, std::string_view key)
{
    return std::ranges::any_of(result.resources, [&](const LoadedResource& resource) {
        return resource.key == key;
    });
}

// Ссылки ресурсов как их разрешает кэш: ID ключа - kFirstLinkId плюс номер
// ресурса в файле. Возвращает ID по ключу.
std::map<std::string, ResourceId> resolveLinks(const LoadResult& result)
{
    constexpr ResourceId kFirstLinkId = 1000;
    std::map<std::string, ResourceId> ids;
    for (std::size_t i = 0; i < result.resources.size(); ++i) {
        ids[result.resources[i].key] = kFirstLinkId + i;
    }
    for (const LoadedResource& resource : result.resources) {
        for (const NestedLink& link : resource.links) {
            link.resolve(ids.at(link.key));
        }
    }

    return ids;
}

// Квадрат в плоскости XY лицом к +Z с uv0 и материалом 0.
void addTexturedQuad(GltfBuilder& gltf, Json extraAttributes = Json::object())
{
    const int positions = gltf.positions({0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0});
    const int normals = gltf.accessor(
        gltf.view(std::vector<float>{0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1}),
        0,
        kFloat,
        4,
        "VEC3"
    );
    const int uvs = gltf.accessor(gltf.view(std::vector<float>{0, 1, 1, 1, 1, 0, 0, 0}), 0, kFloat, 4, "VEC2");
    const int indices = gltf.accessor(
        gltf.view(std::vector<std::uint16_t>{0, 1, 2, 0, 2, 3}),
        0,
        kUnsignedShort,
        6,
        "SCALAR"
    );
    Json attributes{{"POSITION", positions}, {"NORMAL", normals}, {"TEXCOORD_0", uvs}};
    attributes.update(extraAttributes);
    gltf.json["meshes"] = {
        {{"name", "Quad"}, {"primitives", {{{"attributes", attributes}, {"indices", indices}, {"material", 0}}}}}
    };
    gltf.json["nodes"] = {{{"mesh", 0}}};
}

const std::vector<float>& value(const MaterialData& material, const std::string& field)
{
    return material.values.at(field);
}

} // namespace

TEST_CASE("GltfLoader: embedded buffer, no indices, no normals: normals are computed, indices go in order")
{
    const fs::path file = test::freshDirectory("gltf_plain") / "tri.gltf";
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
    const fs::path file = test::freshDirectory("gltf_glb") / "house.glb";
    gltf.writeGlb(file);

    const LoadResult result = load(file);
    REQUIRE(result.error.empty());
    // Модель, три меша, два материала.
    REQUIRE(result.resources.size() == 6);
    const auto& model = data<ModelData>(result.resources[0]);
    CHECK(model.meshes == std::vector<std::string>{"mesh:Door", "mesh:Door#1", "mesh:#2"});
    CHECK(model.materials == std::vector<std::string>{"material:Wood", "material:Metal"});
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
    CHECK(door.submeshes[0].sourceMaterialName == "Metal");
    CHECK_FALSE(door.submeshes[1].sourceMaterial);
    CHECK(door.submeshes[1].sourceMaterialName.empty());
    CHECK(door.submeshes[1].firstIndex == 6);
    CHECK(door.indices[6] == 4);
    CHECK(data<MeshData>(result.resources[3]).submeshes[0].sourceMaterial == 0);
}

TEST_CASE("GltfLoader: an external buffer next to the file, in a folder with a non-ASCII name")
{
    const fs::path directory = test::freshDirectory("gltf_external") / fromUtf8("модели");
    fs::create_directories(directory);
    plainTriangle().writeGltfWithBin(directory / "tri.gltf", "tri.bin");

    const LoadResult result = load(directory / "tri.gltf");
    REQUIRE(result.error.empty());
    CHECK(data<MeshData>(result.resources[1]).vertices[1].position == Vec3(1.0f, 0.0f, 0.0f));
}

TEST_CASE("GltfLoader: an external buffer is a dependency, and when it is missing too")
{
    const fs::path directory = test::freshDirectory("gltf_dependency") / fromUtf8("модели");
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
    const fs::path file = test::freshDirectory("gltf_sparse") / "sparse.gltf";
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
    const fs::path file = test::freshDirectory("gltf_scale") / "tri.gltf";
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
    const fs::path directory = test::freshDirectory("gltf_broken");
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

TEST_CASE("GltfLoader: a GLB with an embedded image and a material gives a nested texture and material")
{
    GltfBuilder gltf;
    const Rgba red{255, 0, 0, 255};
    const int base = gltf.texture(
        tga(1, 1, {red}),
        "Albedo",
        {{"magFilter", 9728}, {"minFilter", 9729}, {"wrapS", 33071}, {"wrapT", 33071}}
    );
    const int metallicRoughness = gltf.texture(tga(1, 1, {{0, 128, 255, 255}}), "MR");
    const int emissive = gltf.texture(tga(1, 1, {red}), "Glow");
    gltf.json["materials"] = {{
        {"name", "Leaf"},
        {"pbrMetallicRoughness",
         {{"baseColorFactor", {0.5, 1.0, 1.0, 0.75}},
          {"baseColorTexture", {{"index", base}}},
          {"metallicFactor", 0.25},
          {"roughnessFactor", 0.5},
          {"metallicRoughnessTexture", {{"index", metallicRoughness}}}}},
        {"emissiveFactor", {0.5, 0.25, 0.0}},
        {"emissiveTexture", {{"index", emissive}}},
        {"extensions", {{"KHR_materials_emissive_strength", {{"emissiveStrength", 4.0}}}}},
        {"alphaMode", "MASK"},
        {"alphaCutoff", 0.3},
        {"doubleSided", true},
    }};
    gltf.json["extensionsUsed"] = {"KHR_materials_emissive_strength"};
    addTexturedQuad(gltf);
    const fs::path file = test::freshDirectory("gltf_material") / "leaf.glb";
    gltf.writeGlb(file);

    const LoadResult result = load(file);
    REQUIRE(result.error.empty());
    CHECK(data<ModelData>(result.resources[0]).materials == std::vector<std::string>{"material:Leaf"});
    const auto& albedo = data<TextureData>(byKey(result, "texture:Albedo"));
    CHECK(albedo.width == 1);
    CHECK(albedo.pixels == std::vector<std::uint8_t>{255, 0, 0, 255});
    CHECK(albedo.sampling.colorSpace == ColorSpace::Srgb);
    CHECK(albedo.sampling.filter == TextureFilter::Nearest);
    CHECK(albedo.sampling.wrap == TextureWrap::Clamp);
    CHECK_FALSE(albedo.sampling.mipmaps);
    CHECK(data<TextureData>(byKey(result, "texture:MR")).sampling.colorSpace == ColorSpace::Linear);
    CHECK(data<TextureData>(byKey(result, "texture:Glow")).sampling.colorSpace == ColorSpace::Srgb);

    const auto ids = resolveLinks(result);
    const auto& material = data<MaterialData>(byKey(result, "material:Leaf"));
    CHECK(material.shader == builtin::kStandardShader);
    // Цвет glTF линейный, у материала - sRGB; альфа как есть.
    CHECK(value(material, "baseColor")[0] == doctest::Approx(math::linearToSrgb(0.5f)));
    CHECK(value(material, "baseColor")[3] == doctest::Approx(0.75f));
    CHECK(value(material, "metallic")[0] == doctest::Approx(0.25f));
    CHECK(value(material, "roughness")[0] == doctest::Approx(0.5f));
    // Затенения нет, а R картинки MR - не затенение.
    CHECK(value(material, "occlusionStrength")[0] == doctest::Approx(0.0f));
    // Свечение: цвет делится на наибольшую составляющую, она - в яркость с
    // силой расширения; 1 - белый на экране.
    CHECK(value(material, "emission")[0] == doctest::Approx(1.0f));
    CHECK(value(material, "emission")[1] == doctest::Approx(math::linearToSrgb(0.5f)));
    CHECK(value(material, "emissionIntensity")[0] == doctest::Approx(2.0f));
    CHECK(value(material, "emissionExposureWeight")[0] == doctest::Approx(0.0f));
    // MASK - непрозрачный с порогом, а не просто непрозрачный.
    CHECK(material.blend == MaterialBlend::Opaque);
    CHECK(value(material, "alphaCutoff")[0] == doctest::Approx(0.3f));
    CHECK(material.cull == MaterialCull::None);
    CHECK(material.textures.at("baseColorMap") == ids.at("texture:Albedo"));
    CHECK(material.textures.at("ormMap") == ids.at("texture:MR"));
    CHECK(material.textures.at("emissionMap") == ids.at("texture:Glow"));
    CHECK_FALSE(material.textures.contains("normalMap"));
}

TEST_CASE("GltfLoader: separate occlusion and metallic-roughness images are packed into one ormMap")
{
    GltfBuilder gltf;
    // Затенение 2 x 1, металл и шероховатость 1 x 1: меньшая растягивается.
    const int occlusion = gltf.texture(tga(2, 1, {{10, 0, 0, 255}, {20, 0, 0, 255}}), "AO");
    const int metallicRoughness = gltf.texture(tga(1, 1, {{99, 128, 64, 255}}), "MR");
    gltf.json["materials"] = {{
        {"name", "Stone"},
        {"pbrMetallicRoughness", {{"metallicRoughnessTexture", {{"index", metallicRoughness}}}}},
        {"occlusionTexture", {{"index", occlusion}, {"strength", 0.5}}},
    }};
    addTexturedQuad(gltf);
    const fs::path file = test::freshDirectory("gltf_orm") / "stone.glb";
    gltf.writeGlb(file);

    const LoadResult result = load(file);
    REQUIRE(result.error.empty());
    // Отдельные картинки сами текстурами не стали: нужна только собранная.
    CHECK_FALSE(has(result, "texture:AO"));
    CHECK_FALSE(has(result, "texture:MR"));
    const auto& packed = data<TextureData>(byKey(result, "texture:Stone ORM"));
    CHECK(packed.width == 2);
    CHECK(packed.height == 1);
    CHECK(packed.sampling.colorSpace == ColorSpace::Linear);
    CHECK(packed.pixels == std::vector<std::uint8_t>{10, 128, 64, 255, 20, 128, 64, 255});
    const auto ids = resolveLinks(result);
    const auto& material = data<MaterialData>(byKey(result, "material:Stone"));
    CHECK(material.textures.at("ormMap") == ids.at("texture:Stone ORM"));
    CHECK(value(material, "occlusionStrength")[0] == doctest::Approx(0.5f));
}

TEST_CASE("GltfLoader: one image used as colour and as data gives two textures")
{
    GltfBuilder gltf;
    const int both = gltf.texture(tga(1, 1, {{128, 128, 255, 255}}), "Shared");
    gltf.json["materials"] = {{
        {"pbrMetallicRoughness", {{"baseColorTexture", {{"index", both}}}}},
        {"normalTexture", {{"index", both}, {"scale", 0.5}}},
        {"alphaMode", "BLEND"},
    }};
    addTexturedQuad(gltf);
    const fs::path file = test::freshDirectory("gltf_twin") / "twin.glb";
    gltf.writeGlb(file);

    const LoadResult result = load(file);
    REQUIRE(result.error.empty());
    CHECK(data<TextureData>(byKey(result, "texture:Shared")).sampling.colorSpace == ColorSpace::Srgb);
    CHECK(data<TextureData>(byKey(result, "texture:Shared linear")).sampling.colorSpace == ColorSpace::Linear);
    const auto ids = resolveLinks(result);
    const auto& material = data<MaterialData>(byKey(result, "material:#0"));
    CHECK(material.blend == MaterialBlend::Transparent);
    CHECK(material.textures.at("baseColorMap") == ids.at("texture:Shared"));
    CHECK(material.textures.at("normalMap") == ids.at("texture:Shared linear"));
    CHECK(value(material, "normalScale")[0] == doctest::Approx(0.5f));
}

TEST_CASE("GltfLoader: tangents are kept from TANGENT, and computed for a normal map without them")
{
    GltfBuilder gltf;
    const int normalMap = gltf.texture(tga(1, 1, {{128, 128, 255, 255}}));
    gltf.json["materials"] = {{{"normalTexture", {{"index", normalMap}}}}};
    SUBCASE("TANGENT keeps its w")
    {
        const int tangents = gltf.accessor(
            gltf.view(std::vector<float>{1, 0, 0, -1, 1, 0, 0, -1, 1, 0, 0, -1, 1, 0, 0, -1}),
            0,
            kFloat,
            4,
            "VEC4"
        );
        addTexturedQuad(gltf, {{"TANGENT", tangents}});
        const fs::path file = test::freshDirectory("gltf_tangent_file") / "quad.glb";
        gltf.writeGlb(file);
        const LoadResult result = load(file);
        REQUIRE(result.error.empty());
        for (const Vertex& vertex : data<MeshData>(byKey(result, "mesh:Quad")).vertices) {
            CHECK(vertex.tangent == Vec4(1.0f, 0.0f, 0.0f, -1.0f));
        }
    }
    SUBCASE("computed along u, the bitangent up the image")
    {
        addTexturedQuad(gltf);
        const fs::path file = test::freshDirectory("gltf_tangent_computed") / "quad.glb";
        gltf.writeGlb(file);
        const LoadResult result = load(file);
        REQUIRE(result.error.empty());
        for (const Vertex& vertex : data<MeshData>(byKey(result, "mesh:Quad")).vertices) {
            CHECK(vertex.tangent.x == doctest::Approx(1.0f));
            CHECK(vertex.tangent.w == doctest::Approx(1.0f));
        }
    }
}

TEST_CASE("GltfLoader: required extensions that are not supported fail the load, used ones are ignored")
{
    GltfBuilder gltf = plainTriangle();
    gltf.json["extensionsUsed"] = {"KHR_draco_mesh_compression", "KHR_texture_transform"};
    const fs::path directory = test::freshDirectory("gltf_extensions");
    gltf.writeGltf(directory / "used.gltf");
    CHECK(load(directory / "used.gltf").error.empty());

    gltf.json["extensionsRequired"] = {"KHR_draco_mesh_compression"};
    gltf.writeGltf(directory / "required.gltf");
    const LoadResult required = load(directory / "required.gltf");
    CHECK(required.error.find("KHR_draco_mesh_compression") != std::string::npos);
}

TEST_CASE("GltfLoader: an image file next to the model is a dependency, a missing one leaves the default")
{
    GltfBuilder gltf;
    gltf.json["images"] = {{{"uri", "albedo.tga"}}, {{"uri", "missing.png"}}};
    gltf.json["textures"] = {{{"source", 0}}, {{"source", 1}}};
    gltf.json["materials"] = {{
        {"name", "Painted"},
        {"pbrMetallicRoughness", {{"baseColorTexture", {{"index", 0}}}}},
        {"emissiveFactor", {1.0, 1.0, 1.0}},
        {"emissiveTexture", {{"index", 1}}},
    }};
    addTexturedQuad(gltf);
    const fs::path directory = test::freshDirectory("gltf_image_file");
    const std::vector<std::uint8_t> image = tga(1, 1, {{0, 255, 0, 255}});
    std::ofstream(directory / "albedo.tga", std::ios::binary)
        .write(reinterpret_cast<const char*>(image.data()), static_cast<std::streamsize>(image.size()));
    gltf.writeGltf(directory / "painted.gltf");

    const LoadResult result = load(directory / "painted.gltf");
    REQUIRE(result.error.empty());
    CHECK(std::ranges::find(result.dependencies, directory / "albedo.tga") != result.dependencies.end());
    CHECK(std::ranges::find(result.dependencies, directory / "missing.png") != result.dependencies.end());
    // Имя текстуры без имени картинки - имя её файла.
    CHECK(data<TextureData>(byKey(result, "texture:albedo")).pixels == std::vector<std::uint8_t>{0, 255, 0, 255});
    CHECK_FALSE(has(result, "texture:missing"));
    resolveLinks(result);
    const auto& material = data<MaterialData>(byKey(result, "material:Painted"));
    CHECK(material.textures.contains("baseColorMap"));
    CHECK_FALSE(material.textures.contains("emissionMap"));
}

TEST_CASE("ResourceCache: meshes of a glTF file are nested resources with ids in .meta")
{
    const fs::path directory = test::freshDirectory("gltf_cache");
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

TEST_CASE("ResourceCache: a model material refers to the nested textures by their ids, and keeps them on re-export")
{
    const fs::path directory = test::freshDirectory("gltf_cache_material");
    GltfBuilder gltf;
    const int base = gltf.texture(tga(1, 1, {{255, 255, 255, 255}}), "Albedo");
    gltf.json["materials"] = {{{"name", "Paint"}, {"pbrMetallicRoughness", {{"baseColorTexture", {{"index", base}}}}}}};
    addTexturedQuad(gltf);
    gltf.writeGlb(directory / "box.glb");
    test::EngineRegistry registry;
    REQUIRE(Project::create(directory, "Game", registry));
    ResourceId texture = 0;
    ResourceId material = 0;
    {
        auto project = Project::open(directory, registry);
        REQUIRE(project);
        Input input;
        EngineServices services{input, registry};
        services.resources().setMetaPolicy(MetaPolicy::Import);
        services.setProject(&*project);
        const ResourceId file = project->resources().id("box.glb");
        REQUIRE(services.resources().find<ModelData>(file));
        const auto& nested = project->resources().subresources(file);
        texture = nested.at("texture:Albedo");
        material = nested.at("material:Paint");
        const MaterialData* paint = services.resources().find<MaterialData>(material);
        REQUIRE(paint);
        CHECK(paint->textures.at("baseColorMap") == texture);
        REQUIRE(services.resources().find<TextureData>(texture));

        // Переэкспорт с другим порядком картинок: ID те же.
        GltfBuilder again;
        again.texture(tga(1, 1, {{0, 0, 0, 255}}), "Unused");
        const int moved = again.texture(tga(1, 1, {{255, 255, 255, 255}}), "Albedo");
        again.json["materials"] = {
            {{"name", "Paint"}, {"pbrMetallicRoughness", {{"baseColorTexture", {{"index", moved}}}}}}
        };
        addTexturedQuad(again);
        again.writeGlb(directory / "box.glb");
        services.resources().reload(file);
        CHECK(services.resources().find<MaterialData>(material)->textures.at("baseColorMap") == texture);
        services.setProject(nullptr);
    }
    // Только чтение: в .meta нет ID новой текстуры - ссылка пустая, материал жив.
    GltfBuilder third;
    const int fresh = third.texture(tga(1, 1, {{255, 255, 255, 255}}), "Fresh");
    third.json["materials"] = {
        {{"name", "Paint"}, {"pbrMetallicRoughness", {{"baseColorTexture", {{"index", fresh}}}}}}
    };
    addTexturedQuad(third);
    third.writeGlb(directory / "box.glb");
    auto project = Project::open(directory, registry);
    REQUIRE(project);
    Input input;
    EngineServices services{input, registry};
    services.setProject(&*project);
    const MaterialData* readOnly = services.resources().find<MaterialData>(material);
    REQUIRE(readOnly);
    CHECK_FALSE(readOnly->textures.contains("baseColorMap"));
    services.setProject(nullptr);
}
