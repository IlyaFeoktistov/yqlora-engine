#include "EngineRegistry.hpp"
#include "TestFiles.hpp"

#include <doctest/doctest.h>

#include <yqlora/editor/ProjectFiles.hpp>
#include <yqlora/import/ShaderLoader.hpp>
#include <yqlora/project/Project.hpp>
#include <yqlora/render/EngineUniforms.hpp>
#include <yqlora/resource/BuiltinResources.hpp>
#include <yqlora/resource/ResourceCache.hpp>
#include <yqlora/resource/ResourceMeta.hpp>
#include <yqlora/shader/ShaderAnnotations.hpp>
#include <yqlora/shader/ShaderDataBuild.hpp>
#include <yqlora/shader/ShaderProfile.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <string>

using namespace yqlora;
namespace fs = std::filesystem;

namespace {

// Шейдер с блоком Material и текстурой; block - поля блока со своими пометками.
std::string shaderWith(const std::string& block, const std::string& texture = "")
{
    return "#version 450\n"
           "#include <yqlora/standard.glsl>\n"
           "layout(set = 2, binding = 0, std140) uniform Material {\n"
           + block + "} material;\n" + texture
           + "#ifdef YQLORA_VERTEX\n"
             "void main() { standardVertex(); }\n"
             "#endif\n"
             "#ifdef YQLORA_FRAGMENT\n"
             "void main() { fragColor = material.baseColor * material.roughness; }\n"
             "#endif\n";
}

ShaderDataResult build(const std::string& source)
{
    const std::map<std::string, std::string> files{{"a.yshader", source}};
    ShaderCompiler compiler([&](const std::string& path) -> std::optional<std::string> {
        const auto it = files.find(path);

        return it != files.end() ? std::optional(it->second) : std::nullopt;
    });

    return buildShaderData(compiler, "a.yshader", "a", kEngineBlocks);
}

bool hasError(const ShaderDataResult& result, int line, const std::string& part)
{
    return std::ranges::any_of(result.errors, [&](const ShaderMessage& message) {
        return message.line == line && message.text.find(part) != std::string::npos;
    });
}

struct ProjectFixture {
    ProjectFixture()
    {
        directory = test::freshDirectory("shader_resources");
        REQUIRE(Project::create(directory, "Game", registry));
        project.emplace(*Project::open(directory, registry));
        cache.addLoader(std::make_unique<ShaderLoader>());
        cache.setMetaPolicy(MetaPolicy::Import);
        cache.setProject(&*project);
    }

    ResourceId add(const fs::path& relative, std::string_view content)
    {
        test::writeFile(directory / relative, content);

        return project->resources().add(relative);
    }

    test::EngineRegistry registry;
    fs::path directory;
    std::optional<Project> project;
    ResourceCache cache;
};

} // namespace

TEST_CASE("Shader annotations: on their own lines and at the end of a declaration")
{
    const AnnotationParseResult parsed = parseShaderAnnotations(
        "uniform Material {\n"
        "    //@ label(\"Roughness\") range(0, 1)\n"
        "\n"
        "    // обычный комментарий не мешает\n"
        "    float roughness;  //@ default(0.5)\n"
        "    vec4 tint;\n"
        "} material;\n"
        "layout(set = 2, binding = 1) uniform sampler2D albedo; //@ default(white) hidden\n",
        "a.yshader"
    );
    CHECK(parsed.errors.empty());
    REQUIRE(parsed.declarations.size() == 2);
    const AnnotatedDeclaration& roughness = parsed.declarations[0];
    CHECK(roughness.name == "roughness");
    CHECK_FALSE(roughness.texture);
    REQUIRE(roughness.annotations.size() == 3);
    CHECK(roughness.annotations[0].word == "label");
    CHECK(std::get<std::string>(roughness.annotations[0].arguments[0]) == "Roughness");
    CHECK(roughness.annotations[1].arguments.size() == 2);
    CHECK(std::get<double>(roughness.annotations[2].arguments[0]) == doctest::Approx(0.5));
    const AnnotatedDeclaration& albedo = parsed.declarations[1];
    CHECK(albedo.texture);
    CHECK(std::get<AnnotationWord>(albedo.annotations[0].arguments[0]).text == "white");
    CHECK(albedo.annotations[1].word == "hidden");
}

TEST_CASE("Shader annotations: an annotation with nothing to describe is an error with its line")
{
    const AnnotationParseResult dangling = parseShaderAnnotations("//@ hidden\nvoid main() {}\n", "a.yshader");
    REQUIRE(dangling.errors.size() == 1);
    CHECK(dangling.errors[0].line == 1);
    const AnnotationParseResult broken = parseShaderAnnotations(
        "uniform Material {\n    float a; //@ range(0, \n} m;\n",
        "a.yshader"
    );
    REQUIRE_FALSE(broken.errors.empty());
    CHECK(broken.errors[0].line == 2);
}

TEST_CASE("Shader data: the material layout comes from the reflection, its look from the annotations")
{
    const ShaderDataResult result = build(shaderWith(
        "    vec4 baseColor;  //@ color default(1, 0.5, 0, 1)\n"
        "    //@ label(\"Roughness\")\n"
        "    float roughness; //@ range(0, 1) default(0.5)\n",
        "layout(set = 2, binding = 1) uniform sampler2D detail; //@ default(normal)\n"
    ));
    for (const ShaderMessage& error : result.errors) {
        MESSAGE(formatShaderMessage(error));
    }
    REQUIRE(result.data);
    const ShaderData& data = *result.data;
    REQUIRE(data.fields.size() == 2);
    const MaterialField* color = data.findField("baseColor");
    REQUIRE(color);
    CHECK(color->color);
    CHECK(color->defaultValue == std::vector<float>{1.0f, 0.5f, 0.0f, 1.0f});
    const MaterialField* roughness = data.findField("roughness");
    REQUIRE(roughness);
    CHECK(roughness->offset == 16);
    CHECK(roughness->label == "Roughness");
    CHECK(roughness->min == 0.0f);
    CHECK(roughness->max == 1.0f);
    // Текстуры, которой шейдер не читает, в отражении нет.
    CHECK(data.findTexture("detail") == nullptr);
    CHECK(data.materialSize >= 20);
}

TEST_CASE("Shader data: unknown, misused and repeated annotations are rejected with their line")
{
    CHECK(hasError(build(shaderWith("    vec4 baseColor; //@ shiny\n    float roughness;\n")), 4, "unknown"));
    CHECK(hasError(build(shaderWith("    vec4 baseColor; //@ range(0, 1)\n    float roughness;\n")), 4, "only for"));
    CHECK(hasError(build(shaderWith("    vec4 baseColor;\n    float roughness; //@ color\n")), 5, "only for"));
    CHECK(hasError(build(shaderWith("    vec4 baseColor; //@ default(1, 1)\n    float roughness;\n")), 4, "takes 4"));
    CHECK(hasError(build(shaderWith("    vec4 baseColor;\n    float roughness; //@ range(1, 0)\n")), 5, "minimum"));
    CHECK(hasError(build(shaderWith("    vec4 baseColor; //@ hidden hidden\n    float roughness;\n")), 4, "twice"));
    CHECK(hasError(
        build(shaderWith("    vec4 baseColor; //@ label(Roughness)\n    float roughness;\n")),
        4,
        "string in quotes"
    ));
}

TEST_CASE("Shader data: the material set holds only the Material block and textures")
{
    const ShaderDataResult result = build(
        "#version 450\n#include <yqlora/standard.glsl>\n"
        "layout(set = 2, binding = 3, std140) uniform Extra { vec4 color; } extra;\n"
        "#ifdef YQLORA_VERTEX\nvoid main() { standardVertex(); }\n#endif\n"
        "#ifdef YQLORA_FRAGMENT\nvoid main() { fragColor = extra.color; }\n#endif\n"
    );
    CHECK_FALSE(result.data);
    CHECK(hasError(result, 0, "only the Material block"));
}

TEST_CASE("Shader data: the shader template compiles")
{
    ShaderCompiler compiler([](const std::string& path) -> std::optional<std::string> {
        std::ifstream stream(fs::path(YQLORA_ASSETS_DIR) / "templates" / path, std::ios::binary);
        if (!stream) {
            return std::nullopt;
        }

        return std::string(std::istreambuf_iterator<char>(stream), {});
    });
    const ShaderDataResult result = buildShaderData(compiler, "shader/Shader.yshader", "Shader", kEngineBlocks);
    for (const ShaderMessage& error : result.errors) {
        MESSAGE(formatShaderMessage(error));
    }
    REQUIRE(result.data);
    CHECK(result.data->findField("baseColor")->color);
    CHECK(result.data->findTexture("baseColorMap"));
}

TEST_CASE("ResourceCache: built-in shaders are there as resources")
{
    ResourceCache cache;
    const ShaderData* standard = cache.find<ShaderData>(builtin::kStandardShader);
    REQUIRE(standard);
    CHECK(standard->findField("baseColor"));
    CHECK(standard->findField("roughness"));
    CHECK(cache.find<ShaderData>(builtin::kErrorShader));
    CHECK(cache.find<MeshData>(builtin::kStandardShader) == nullptr);
}

TEST_CASE_FIXTURE(ProjectFixture, "ShaderLoader: one file gives both stages, the .meta keeps the profile version")
{
    const ResourceId id = add("shaders/lit.yshader", shaderWith("    vec4 baseColor;\n    float roughness;\n"));
    const ShaderData* shader = cache.find<ShaderData>(id);
    REQUIRE(shader);
    CHECK_FALSE(shader->desc.vertex.empty());
    CHECK_FALSE(shader->desc.fragment.empty());
    CHECK(shader->desc.name == "lit");
    const auto import = nlohmann::json::parse(project->resources().importSettings(id));
    CHECK(import.at("profileVersion") == kShaderProfileVersion);

    // Без импорта (собранная игра) версия не пишется, а шейдер без неё читается.
    const ResourceId plain = add("shaders/plain.yshader", shaderWith("    vec4 baseColor;\n    float roughness;\n"));
    ResourceCache game;
    game.addLoader(std::make_unique<ShaderLoader>());
    game.setProject(&*project);
    CHECK(game.find<ShaderData>(plain));
    CHECK(project->resources().importSettings(plain) == kNoImportSettings);

    // Шейдер под другую версию профиля не загружается, пока проект не переведут.
    REQUIRE(project->resources().setImportSettings(id, R"({"profileVersion": 999})"));
    REQUIRE(cache.reload(id));
    CHECK(cache.status(id)->state == ResourceState::Error);
    CHECK(cache.status(id)->message.find("profile") != std::string::npos);
}

TEST_CASE_FIXTURE(ProjectFixture, "ShaderLoader: an error inside an include names that file and line")
{
    test::writeFile(directory / "shaders" / "common.glsl", "// first line\nfloat broken = undefinedName;\n");
    const ResourceId id = add(
        "shaders/lit.yshader",
        "#version 450\n#include \"common.glsl\"\n#ifdef YQLORA_FRAGMENT\nlayout(location = 0) out vec4 c;\n"
        "void main() { c = vec4(1.0); }\n#else\nvoid main() { gl_Position = vec4(0.0); }\n#endif\n"
    );
    CHECK(cache.find<ShaderData>(id) == nullptr);
    const ResourceStatus* status = cache.status(id);
    REQUIRE(status);
    CHECK(status->state == ResourceState::Error);
    CHECK(status->message.find("shaders/common.glsl:2: ") != std::string::npos);
}

TEST_CASE_FIXTURE(ProjectFixture, "ShaderLoader: a changed include reloads the shaders that use it")
{
    test::writeFile(directory / "shaders" / "common.glsl", "const float kValue = 0.5;\n");
    const ResourceId id = add(
        "shaders/lit.yshader",
        "#version 450\n#include \"common.glsl\"\n#ifdef YQLORA_FRAGMENT\nlayout(location = 0) out vec4 c;\n"
        "void main() { c = vec4(kValue); }\n#else\nvoid main() { gl_Position = vec4(0.0); }\n#endif\n"
    );
    const auto first = cache.get<ShaderData>(id);
    REQUIRE(first);
    CHECK(cache.reloadChanged() == 0);

    test::rewriteFile(directory / "shaders" / "common.glsl", "const float kValue = undefinedName;\n");
    CHECK(cache.reloadChanged() == 1);
    CHECK(cache.status(id)->state == ResourceState::Error);

    // Исправленный файл подхватывается так же.
    test::rewriteFile(directory / "shaders" / "common.glsl", "const float kValue = 0.25;\n");
    CHECK(cache.reloadChanged() == 1);
    const auto fixed = cache.get<ShaderData>(id);
    REQUIRE(fixed);
    CHECK(fixed.revision > first.revision);
}

TEST_CASE_FIXTURE(ProjectFixture, "ProjectFiles: a new shader is the template under the given name")
{
    editor::ProjectFiles files(*project);
    REQUIRE(files.createFolder({}, "shaders"));
    const auto created = files.createShader("shaders", "Water", editor::shaderTemplateFile(YQLORA_ASSETS_DIR));
    REQUIRE(created);
    CHECK(*created == fs::path("shaders") / "Water.yshader");
    CHECK(cache.find<ShaderData>(project->resources().id(*created)));
    CHECK_FALSE(files.createShader("shaders", "Water", editor::shaderTemplateFile(YQLORA_ASSETS_DIR)));
}
