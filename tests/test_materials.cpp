#include "EngineRegistry.hpp"
#include "RecordingDevice.hpp"
#include "TestFiles.hpp"

#include <doctest/doctest.h>

#include <yqlora/core/EngineServices.hpp>
#include <yqlora/import/MaterialLoader.hpp>
#include <yqlora/input/Input.hpp>
#include <yqlora/material/MaterialData.hpp>
#include <yqlora/material/MaterialFile.hpp>
#include <yqlora/material/SharedMaterial.hpp>
#include <yqlora/platform/Files.hpp>
#include <yqlora/project/Project.hpp>
#include <yqlora/render/FrameBuilder.hpp>
#include <yqlora/render/FrameFormats.hpp>
#include <yqlora/render/MaterialBlock.hpp>
#include <yqlora/render/MeshRenderer.hpp>
#include <yqlora/render/PipelineCache.hpp>
#include <yqlora/resource/BuiltinResources.hpp>
#include <yqlora/resource/ResourceCache.hpp>
#include <yqlora/scene/Node3D.hpp>
#include <yqlora/scene/Scene.hpp>
#include <yqlora/shader/ShaderData.hpp>

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

using namespace yqlora;
namespace fs = std::filesystem;
using Json = nlohmann::json;

namespace {

MaterialField floatField(const char* name, float value)
{
    return {.name = name, .type = rhi::ValueType::Float, .defaultValue = {value}};
}

} // namespace

TEST_CASE("Material file: values by name, the state and textures go and come back")
{
    const MaterialData material{
        .shader = 0x1234567890abcdefULL,
        .blend = MaterialBlend::Transparent,
        .cull = MaterialCull::None,
        .values = {{"baseColor", {1.0f, 0.5f, 0.2f, 1.0f}}, {"roughness", {0.4f}}, {"gone", {7.0f}}},
        .textures = {{"baseColorMap", 0x4c1e07a95b3d2f60ULL}},
    };
    const Json json = materialToJson(material);
    CHECK(json.at("blend") == "transparent");
    CHECK(json.at("parameters").at("roughness").is_number());
    std::string error;
    const auto read = materialFromJson(json, error);
    REQUIRE(read);
    CHECK(*read == material);

    CHECK_FALSE(materialFromJson(Json::array(), error));
    Json badBlend = json;
    badBlend["blend"] = "additive";
    CHECK_FALSE(materialFromJson(badBlend, error));
    CHECK(error.find("blend") != std::string::npos);
    Json newer = json;
    newer["version"] = 99;
    CHECK_FALSE(materialFromJson(newer, error));
}

TEST_CASE("Material values: the material's value, the default, a value of another type is not used")
{
    MaterialData material;
    material.values["roughness"] = {0.25f};
    // Поле стало float, а хранится vec4: берётся default, значение остаётся.
    material.values["metallic"] = {1.0f, 1.0f, 1.0f, 1.0f};
    CHECK(materialValue(material, floatField("roughness", 0.5f))[0] == doctest::Approx(0.25f));
    CHECK(materialValue(material, floatField("metallic", 0.0f))[0] == doctest::Approx(0.0f));
    CHECK(materialValue(material, floatField("missing", 0.75f))[0] == doctest::Approx(0.75f));
    CHECK(material.values.contains("metallic"));

    material.textures["detail"] = 42;
    CHECK(materialTexture(material, {.name = "detail"}) == 42);
    CHECK(materialTexture(material, {.name = "other"}) == 0);
}

TEST_CASE("Materials: the built-in ones and a .ymat of the project are resources")
{
    ResourceCache builtins;
    const MaterialData* standard = builtins.find<MaterialData>(builtin::kDefaultMaterial);
    REQUIRE(standard);
    CHECK(standard->shader == builtin::kStandardShader);
    CHECK(standard->values.empty());
    REQUIRE(builtins.find<MaterialData>(builtin::kErrorMaterial));
    CHECK(builtins.find<MaterialData>(builtin::kErrorMaterial)->shader == builtin::kErrorShader);

    const fs::path directory = test::freshDirectory("materials_cache");
    test::EngineRegistry registry;
    REQUIRE(Project::create(directory, "Game", registry));
    auto project = Project::open(directory, registry);
    REQUIRE(project);
    REQUIRE(writeMaterialFile(directory / "red.ymat", {.values = {{"baseColor", {1.0f, 0.0f, 0.0f, 1.0f}}}}));
    test::writeFile(directory / "broken.ymat", R"({"version": 1, "blend": "glow"})");
    const ResourceId red = project->resources().add("red.ymat");
    const ResourceId broken = project->resources().add("broken.ymat");
    Input input;
    EngineServices services{input, registry};
    services.setProject(&*project);

    ResourceCache& cache = services.resources();
    CHECK(cache.kindOf("red.ymat") == MaterialData::kResourceKind);
    const MaterialData* material = cache.find<MaterialData>(red);
    REQUIRE(material);
    CHECK(material->values.at("baseColor")[0] == doctest::Approx(1.0f));
    CHECK(cache.find<MaterialData>(broken) == nullptr);
    CHECK(cache.status(broken)->state == ResourceState::Error);
}

namespace {

// Шейдер с блоком: vec4 baseColor (цвет) по 0, float roughness по 16, int steps по 20.
ShaderData blockShader()
{
    ShaderData shader;
    shader.materialSize = 32;
    shader.fields = {
        {.name = "baseColor",
         .type = rhi::ValueType::Vec4,
         .offset = 0,
         .size = 16,
         .color = true,
         .defaultValue = {1, 1, 1, 1}},
        {.name = "roughness", .type = rhi::ValueType::Float, .offset = 16, .size = 4, .defaultValue = {0.5f}},
        {.name = "steps", .type = rhi::ValueType::Int, .offset = 20, .size = 4, .defaultValue = {8}},
    };

    return shader;
}

template <typename T>
T at(const std::vector<std::byte>& block, std::size_t offset)
{
    T value{};
    std::memcpy(&value, block.data() + offset, sizeof(T));

    return value;
}

} // namespace

TEST_CASE("materialBlock: values by the offsets of the shader, colors linear, ints as ints")
{
    const ShaderData shader = blockShader();
    const MaterialData material{.values = {{"baseColor", {0.5f, 0.5f, 0.5f, 0.25f}}}};
    const auto block = materialBlock(shader, material);
    REQUIRE(block.size() == 32);
    CHECK(at<float>(block, 0) == doctest::Approx(math::srgbToLinear(0.5f)));
    // Альфа не переводится.
    CHECK(at<float>(block, 12) == doctest::Approx(0.25f));
    CHECK(at<float>(block, 16) == doctest::Approx(0.5f));
    CHECK(at<std::int32_t>(block, 20) == 8);
}

TEST_CASE("materialBlock: overrides go over the material, a wrong one is skipped and reported, clear returns it")
{
    const ShaderData shader = blockShader();
    const MaterialData material{.values = {{"roughness", {0.4f}}}};
    MaterialOverrides overrides;
    overrides.setFloat("roughness", 0.1f);
    overrides.setVector("baseColor", Vec4(1.0f)); // не цвет: у цветного поля только setColor
    overrides.setFloat("gone", 3.0f);
    std::vector<std::string> skipped;
    const auto block = materialBlock(shader, material, &overrides, [&](std::string_view name) {
        skipped.emplace_back(name);
    });
    CHECK(at<float>(block, 16) == doctest::Approx(0.1f));
    CHECK(at<float>(block, 0) == doctest::Approx(1.0f));
    CHECK(skipped == std::vector<std::string>{"baseColor", "gone"});

    overrides.setColor("baseColor", Vec4(0.5f, 0.5f, 0.5f, 1.0f));
    CHECK(at<float>(materialBlock(shader, material, &overrides), 0) == doctest::Approx(math::srgbToLinear(0.5f)));
    overrides.clear("roughness");
    CHECK(at<float>(materialBlock(shader, material, &overrides), 16) == doctest::Approx(0.4f));
    overrides.clearAll();
    CHECK(overrides.empty());
}

TEST_CASE("PipelineCache: one pipeline per state, a new shader revision gets new ones")
{
    test::RecordingDevice device;
    PipelineCache pipelines(device);
    ResourceCache cache;
    const LoadedData<ShaderData> standard = cache.get<ShaderData>(builtin::kStandardShader);
    const MaterialData opaque;
    const MaterialData transparent{.blend = MaterialBlend::Transparent};
    FrameMaterial material{
        .id = 1,
        .data = &opaque,
        .shaderId = builtin::kStandardShader,
        .shaderRevision = standard.revision,
        .shader = standard.data,
    };
    const rhi::Pipeline* first = pipelines.get(material, kHdrColorFormat, kDepthFormat);
    REQUIRE(first);
    CHECK(pipelines.get(material, kHdrColorFormat, kDepthFormat) == first);
    // Другой материал с тем же состоянием - тот же конвейер.
    FrameMaterial other = material;
    other.id = 2;
    CHECK(pipelines.get(other, kHdrColorFormat, kDepthFormat) == first);
    other.data = &transparent;
    const rhi::Pipeline* blended = pipelines.get(other, kHdrColorFormat, kDepthFormat);
    CHECK(blended != first);
    CHECK_FALSE(blended->desc().depthWrite);
    CHECK(pipelines.size() == 2);

    material.shaderRevision += 1;
    CHECK(pipelines.get(material, kHdrColorFormat, kDepthFormat) != nullptr);
    CHECK(pipelines.size() == 1);
}

TEST_CASE("Extract: a slot draws its material, a broken shader draws Error until it is fixed, overrides are copied")
{
    const fs::path directory = test::freshDirectory("materials_extract");
    test::EngineRegistry registry;
    REQUIRE(Project::create(directory, "Game", registry));
    auto project = Project::open(directory, registry);
    REQUIRE(project);
    const std::string shaderText = *platform::readFile(fs::path(YQLORA_ASSETS_DIR) / "templates/shader/Shader.yshader");
    test::writeFile(directory / "lit.yshader", shaderText);
    const ResourceId shader = project->resources().add("lit.yshader");
    REQUIRE(writeMaterialFile(directory / "lit.ymat", {.shader = shader}));
    const ResourceId material = project->resources().add("lit.ymat");
    Input input;
    EngineServices services{input, registry};
    services.setProject(&*project);

    Scene scene{services};
    auto& root = scene.createRoot<Node3D>("Root");
    auto& renderer = root.addComponent<MeshRenderer>();
    renderer.setMesh({builtin::kCubeMesh});
    renderer.setMaterials({{material}});
    const auto drawn = [&] {
        const FrameData frame = extractFrame(scene, 1.0f);
        REQUIRE(frame.drawItems.size() == 1);

        return frame.materials[frame.drawItems[0].material];
    };
    CHECK(drawn().id == material);
    CHECK(drawn().shaderId == shader);

    test::rewriteFile(directory / "lit.yshader", "#version 450\nvoid main() { broken }\n");
    services.resources().reloadChanged();
    CHECK(drawn().id == builtin::kErrorMaterial);
    test::rewriteFile(directory / "lit.yshader", shaderText);
    services.resources().reloadChanged();
    CHECK(drawn().id == material);

    renderer.setMaterials({});
    CHECK(drawn().id == builtin::kDefaultMaterial);
    renderer.overrides(0).setFloat("roughness", 0.1f);
    const FrameData frame = extractFrame(scene, 1.0f);
    REQUIRE(frame.overrides.size() == 1);
    CHECK(frame.drawItems[0].overrides == 0);
    CHECK(frame.overrides[0].values == renderer.overrides(0));
    CHECK(frame.overrides[0].textures.size() == frame.materials[0].textures.size());
}

TEST_CASE(
    "SharedMaterial: a change reaches every object at once with a new revision, the file stays, built-ins do not change"
)
{
    const fs::path directory = test::freshDirectory("materials_shared");
    test::EngineRegistry registry;
    REQUIRE(Project::create(directory, "Game", registry));
    auto project = Project::open(directory, registry);
    REQUIRE(project);
    REQUIRE(writeMaterialFile(directory / "red.ymat", {.values = {{"roughness", {0.5f}}}}));
    const ResourceId red = project->resources().add("red.ymat");
    Input input;
    EngineServices services{input, registry};
    services.setProject(&*project);
    ResourceCache& cache = services.resources();
    const std::uint64_t before = cache.get<MaterialData>(red).revision;
    const std::string file = *platform::readFile(directory / "red.ymat");

    SharedMaterial material(services, {red});
    REQUIRE(material.setFloat("roughness", 0.1f));
    REQUIRE(material.setTexture("baseColorMap", {builtin::kFirstProjectId + 1}));
    const LoadedData<MaterialData> after = cache.get<MaterialData>(red);
    CHECK(after.revision > before);
    CHECK(after.data->values.at("roughness")[0] == doctest::Approx(0.1f));
    CHECK(after.data->textures.at("baseColorMap") == builtin::kFirstProjectId + 1);
    CHECK(*platform::readFile(directory / "red.ymat") == file);

    CHECK_FALSE(SharedMaterial(services, {builtin::kDefaultMaterial}).setFloat("roughness", 0.1f));
    CHECK_FALSE(SharedMaterial(services, {123}).setFloat("roughness", 0.1f));
}
