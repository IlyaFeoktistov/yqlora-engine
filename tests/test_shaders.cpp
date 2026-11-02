#include <doctest/doctest.h>

#include <yqlora/editor/render/EditorShaders.hpp>

#include <yqlora/render/ShaderData.hpp>
#include <yqlora/shader/EngineShaders.hpp>
#include <yqlora/shader/ShaderBuild.hpp>
#include <yqlora/shader/ShaderCompiler.hpp>
#include <yqlora/shader/ShaderProfile.hpp>
#include <yqlora/shader/UniformLayout.hpp>

#if YQLORA_HAS_OPENGL
#include <yqlora/opengl/GlslTranslator.hpp>
#endif

#include <algorithm>
#include <map>
#include <string>

using namespace yqlora;

namespace {

// Файлы шейдеров проекта в памяти: путь -> текст.
struct Files {
    std::map<std::string, std::string> files;

    ShaderFileReader reader()
    {
        return [this](const std::string& path) -> std::optional<std::string> {
            const auto it = files.find(path);
            if (it == files.end()) {
                return std::nullopt;
            }

            return it->second;
        };
    }
};

const char* const kPlainVertex = R"(#version 450
layout(location = 0) in vec3 aPosition;
void main()
{
    gl_Position = vec4(aPosition, 1.0);
}
)";

const char* const kPlainFragment = R"(#version 450
layout(location = 0) out vec4 fragColor;
void main()
{
    fragColor = vec4(1.0);
}
)";

bool hasError(const std::vector<ShaderMessage>& errors, const std::string& part)
{
    return std::ranges::any_of(errors, [&](const ShaderMessage& message) {
        return message.text.find(part) != std::string::npos;
    });
}

ShaderBuildResult buildWithFragment(Files& files, const std::string& fragment)
{
    files.files["a.vert"] = kPlainVertex;
    files.files["a.frag"] = fragment;
    ShaderCompiler compiler(files.reader());

    return buildShader(compiler, {.name = "a", .vertexPath = "a.vert", .fragmentPath = "a.frag"});
}

struct BuiltinShader {
    std::string name;
    std::string vertex;
    std::string fragment;
};

// Встроенные шейдеры движка и редактора; компилятор с readEditorShader видит оба.
const BuiltinShader kBuiltinShaders[] = {
    {"mesh", engineShaderPath("mesh.vert"), engineShaderPath("mesh.frag")},
    {"grid", editor::editorShaderPath("grid.vert"), editor::editorShaderPath("grid.frag")},
    {"outline mask", editor::editorShaderPath("outline_mask.vert"), editor::editorShaderPath("outline_mask.frag")},
    {"outline", engineShaderPath("fullscreen.vert"), editor::editorShaderPath("outline.frag")},
};

} // namespace

TEST_CASE("Shaders: every built-in shader builds, fits the profile and matches the C++ blocks")
{
    ShaderCompiler compiler(editor::readEditorShader);
    for (const BuiltinShader& shader : kBuiltinShaders) {
        CAPTURE(shader.name);
        const ShaderBuildResult result = buildShader(
            compiler,
            {
                .name = shader.name,
                .vertexPath = shader.vertex,
                .fragmentPath = shader.fragment,
                .layouts = kEngineBlocks,
            }
        );
        for (const ShaderMessage& error : result.errors) {
            MESSAGE(formatShaderMessage(error));
        }
        REQUIRE(result.desc);
        CHECK_FALSE(result.desc->vertex.empty());
        CHECK_FALSE(result.desc->fragment.empty());
    }
}

TEST_CASE("Shaders: reflection of the mesh shader")
{
    ShaderCompiler compiler({});
    const rhi::ShaderDesc desc = buildBuiltinShader(
        compiler,
        {.name = "mesh", .vertexPath = engineShaderPath("mesh.vert"), .fragmentPath = engineShaderPath("mesh.frag")}
    );
    const rhi::ShaderReflection& reflection = desc.reflection;

    const rhi::ShaderBinding* object = reflection.find(kObjectBlock.slot);
    REQUIRE(object);
    CHECK(object->name == "Object");
    CHECK(object->kind == rhi::BindingKind::UniformBlock);
    CHECK(object->stages == (rhi::stageBit(rhi::ShaderStage::Vertex) | rhi::stageBit(rhi::ShaderStage::Fragment)));
    REQUIRE(object->fields.size() == 3);
    CHECK(object->fields[2].name == "color");
    CHECK(object->fields[2].type == rhi::ValueType::Vec4);
    CHECK(object->fields[2].offset == offsetof(ObjectUniforms, color));

    const rhi::ShaderBinding* frame = reflection.find(kFrameBlock.slot);
    REQUIRE(frame);
    CHECK(frame->stages == rhi::stageBit(rhi::ShaderStage::Fragment));

    REQUIRE(reflection.vertexInputs.size() == 2);
    const auto normal = std::ranges::find(reflection.vertexInputs, kNormalLocation, &rhi::VertexInput::location);
    REQUIRE(normal != reflection.vertexInputs.end());
    CHECK(normal->type == rhi::ValueType::Vec3);
}

TEST_CASE("Shaders: a shader keeps only the blocks it uses")
{
    // Шейдер сетки подключает все блоки движка, но читает только вид и свой.
    ShaderCompiler compiler(editor::readEditorShader);
    const rhi::ShaderDesc desc = buildBuiltinShader(
        compiler,
        {
            .name = "grid",
            .vertexPath = editor::editorShaderPath("grid.vert"),
            .fragmentPath = editor::editorShaderPath("grid.frag"),
        }
    );
    CHECK(desc.reflection.find(kViewBlock.slot));
    CHECK(desc.reflection.find({kMaterialSet, 0}));
    CHECK_FALSE(desc.reflection.find(kFrameBlock.slot));
    CHECK_FALSE(desc.reflection.find(kObjectBlock.slot));
}

TEST_CASE("Shaders: an error names the file and the line, also inside an include")
{
    Files files;
    files.files["shaders/a.frag"] = "#version 450\n#include \"common/b.glsl\"\nvoid main() {}\n";
    files.files["shaders/common/b.glsl"] = "// first line\nfloat broken = undefinedName;\n";
    ShaderCompiler compiler(files.reader());

    const ShaderCompileResult result = compiler.compile(
        {.path = "shaders/a.frag", .stage = rhi::ShaderStage::Fragment}
    );
    REQUIRE_FALSE(result.ok());
    CHECK(result.errors.front().file == "shaders/common/b.glsl");
    CHECK(result.errors.front().line == 2);
    CHECK(result.errors.front().text.find("undefinedName") != std::string::npos);
    CHECK(formatShaderMessage(result.errors.front()).starts_with("shaders/common/b.glsl:2: "));
}

TEST_CASE("Shaders: a missing file and a missing include are errors")
{
    Files files;
    ShaderCompiler compiler(files.reader());
    const ShaderCompileResult missing = compiler.compile({.path = "none.frag", .stage = rhi::ShaderStage::Fragment});
    REQUIRE_FALSE(missing.ok());
    CHECK(missing.errors.front().file == "none.frag");

    files.files["a.frag"] = "#version 450\n#include \"none.glsl\"\nvoid main() {}\n";
    const ShaderCompileResult include = compiler.compile({.path = "a.frag", .stage = rhi::ShaderStage::Fragment});
    REQUIRE_FALSE(include.ok());
    CHECK(include.errors.front().file == "a.frag");
    CHECK(include.errors.front().line == 2);
}

TEST_CASE("Shaders: the engine library is included with angle brackets")
{
    Files files;
    files.files["a.frag"] = "#version 450\n#include <yqlora/uniforms.glsl>\n"
                            "layout(location = 0) out vec4 c;\nvoid main() { c = object.color; }\n";
    ShaderCompiler compiler(files.reader());
    CHECK(compiler.compile({.path = "a.frag", .stage = rhi::ShaderStage::Fragment}).ok());
}

TEST_CASE("Shaders: defines reach the source")
{
    Files files;
    files.files["a.frag"] = "#version 450\nlayout(location = 0) out vec4 c;\n"
                            "void main() { c = vec4(VALUE); }\n";
    ShaderCompiler compiler(files.reader());
    CHECK_FALSE(compiler.compile({.path = "a.frag", .stage = rhi::ShaderStage::Fragment}).ok());
    CHECK(
        compiler.compile({.path = "a.frag", .stage = rhi::ShaderStage::Fragment, .defines = {{"VALUE", "0.5"}}}).ok()
    );
}

TEST_CASE("Shaders: the cache compiles a file again only when it or an include changed")
{
    Files files;
    files.files["a.frag"] = "#version 450\n#include \"b.glsl\"\nlayout(location = 0) out vec4 c;\n"
                            "void main() { c = vec4(kValue); }\n";
    files.files["b.glsl"] = "const float kValue = 0.5;\n";
    ShaderCompiler compiler(files.reader());
    const ShaderCompileRequest request{.path = "a.frag", .stage = rhi::ShaderStage::Fragment};

    const ShaderCompileResult first = compiler.compile(request);
    REQUIRE(first.ok());
    CHECK(compiler.compile(request).spirv == first.spirv);
    CHECK(compiler.compilations() == 1);

    files.files["b.glsl"] = "const float kValue = 0.25;\n";
    CHECK(compiler.compile(request).ok());
    CHECK(compiler.compilations() == 2);

    // Другие определения - другой результат.
    CHECK(compiler.compile({.path = "a.frag", .stage = rhi::ShaderStage::Fragment, .defines = {{"X", "1"}}}).ok());
    CHECK(compiler.compilations() == 3);
}

TEST_CASE("Shaders: resources outside the profile are rejected")
{
    Files files;
    SUBCASE("storage buffer")
    {
        const ShaderBuildResult result = buildWithFragment(
            files,
            "#version 450\nlayout(set = 2, binding = 0, std430) buffer Data { vec4 values[]; } data;\n"
            "layout(location = 0) out vec4 c;\nvoid main() { c = data.values[0]; }\n"
        );
        CHECK_FALSE(result.desc);
        CHECK(hasError(result.errors, "storage buffer"));
    }
    SUBCASE("set past the object set")
    {
        const ShaderBuildResult result = buildWithFragment(
            files,
            "#version 450\nlayout(set = 4, binding = 0, std140) uniform Extra { vec4 color; } extra;\n"
            "layout(location = 0) out vec4 c;\nvoid main() { c = extra.color; }\n"
        );
        CHECK_FALSE(result.desc);
        CHECK(hasError(result.errors, "set 4"));
    }
    SUBCASE("too many textures")
    {
        std::string source = "#version 450\nlayout(location = 0) out vec4 c;\n";
        std::string sum = "vec4(0.0)";
        for (std::size_t i = 0; i <= kMaxShaderTextures; ++i) {
            const std::string name = "t" + std::to_string(i);
            source += "layout(set = 2, binding = " + std::to_string(i) + ") uniform sampler2D " + name + ";\n";
            sum += " + texture(" + name + ", vec2(0.0))";
        }
        source += "void main() { c = " + sum + "; }\n";
        const ShaderBuildResult result = buildWithFragment(files, source);
        CHECK_FALSE(result.desc);
        CHECK(hasError(result.errors, "textures"));
    }
    SUBCASE("vertex input at an unknown location")
    {
        files.files["a.vert"] = "#version 450\nlayout(location = 5) in vec3 aOther;\n"
                                "void main() { gl_Position = vec4(aOther, 1.0); }\n";
        files.files["a.frag"] = kPlainFragment;
        ShaderCompiler compiler(files.reader());
        const ShaderBuildResult result = buildShader(
            compiler,
            {.name = "a", .vertexPath = "a.vert", .fragmentPath = "a.frag"}
        );
        CHECK_FALSE(result.desc);
        CHECK(hasError(result.errors, "location 5"));
    }
}

TEST_CASE("Shaders: a block that differs from its C++ struct is rejected")
{
    struct alignas(16) Tint {
        Vec4 color{1.0f};
        float strength = 1.0f;
    };
    const UniformLayout tint{
        "Tint",
        {kMaterialSet, 0},
        sizeof(Tint),
        {YQLORA_UNIFORM_MEMBER(Tint, color), YQLORA_UNIFORM_MEMBER(Tint, strength)},
    };
    const UniformLayout* const layouts[] = {&tint};

    Files files;
    files.files["a.vert"] = kPlainVertex;
    const auto build = [&](const std::string& block) {
        files.files["a.frag"] = "#version 450\nlayout(set = 2, binding = 0, std140) uniform Tint {\n" + block
                                + "} tint;\nlayout(location = 0) out vec4 c;\nvoid main() { c = tint.color; }\n";
        ShaderCompiler compiler(files.reader());

        return buildShader(
            compiler,
            {.name = "a", .vertexPath = "a.vert", .fragmentPath = "a.frag", .layouts = layouts}
        );
    };

    CHECK(build("vec4 color;\nfloat strength;\n").desc);
    // Шейдер может объявить не все поля.
    CHECK(build("vec4 color;\n").desc);

    const ShaderBuildResult swapped = build("float strength;\nvec4 color;\n");
    CHECK_FALSE(swapped.desc);
    CHECK(hasError(swapped.errors, "'color' at offset 16"));

    const ShaderBuildResult unknown = build("vec4 color;\nfloat other;\n");
    CHECK_FALSE(unknown.desc);
    CHECK(hasError(unknown.errors, "'other' is not in the C++ struct"));

    const ShaderBuildResult resized = build("vec4 color;\nvec2 strength;\n");
    CHECK_FALSE(resized.desc);
    CHECK(hasError(resized.errors, "'strength' is 8 bytes"));
}

TEST_CASE("Shaders: both stages must declare a shared resource the same way")
{
    Files files;
    files.files["a.vert"] = "#version 450\nlayout(set = 2, binding = 0, std140) uniform Tint { vec4 color; } tint;\n"
                            "void main() { gl_Position = tint.color; }\n";
    files.files["a.frag"] = "#version 450\nlayout(set = 2, binding = 0, std140) uniform Tint { vec2 color; } tint;\n"
                            "layout(location = 0) out vec4 c;\nvoid main() { c = vec4(tint.color, 0.0, 1.0); }\n";
    ShaderCompiler compiler(files.reader());
    const ShaderBuildResult result = buildShader(
        compiler,
        {.name = "a", .vertexPath = "a.vert", .fragmentPath = "a.frag"}
    );
    CHECK_FALSE(result.desc);
    CHECK(hasError(result.errors, "declared differently"));
}

#if YQLORA_HAS_OPENGL
TEST_CASE("Shaders: built-in shaders translate to GLSL 330 for OpenGL")
{
    ShaderCompiler compiler(editor::readEditorShader);
    for (const BuiltinShader& shader : kBuiltinShaders) {
        CAPTURE(shader.name);
        const rhi::ShaderDesc desc = buildBuiltinShader(
            compiler,
            {
                .name = shader.name,
                .vertexPath = shader.vertex,
                .fragmentPath = shader.fragment,
            }
        );
        const std::string vertex = opengl::translateToGlsl330(desc.vertex, rhi::ShaderStage::Vertex);
        const std::string fragment = opengl::translateToGlsl330(desc.fragment, rhi::ShaderStage::Fragment);
        CHECK(vertex.starts_with("#version 330"));
        CHECK(fragment.starts_with("#version 330"));
        // В OpenGL 3.3 нет привязок в шейдере: ресурсы ищутся по именам.
        CHECK(vertex.find("binding =") == std::string::npos);
        CHECK(fragment.find("binding =") == std::string::npos);
        for (const rhi::ShaderBinding& binding : desc.reflection.bindings) {
            const std::string name = opengl::glResourceName(binding.slot);
            const bool found = vertex.find(name) != std::string::npos || fragment.find(name) != std::string::npos;
            CHECK(found);
        }
    }
}

TEST_CASE("Shaders: the OpenGL vertex stage maps clip depth 0..1 to -1..1")
{
    ShaderCompiler compiler({});
    const rhi::ShaderDesc desc = buildBuiltinShader(
        compiler,
        {.name = "mesh", .vertexPath = engineShaderPath("mesh.vert"), .fragmentPath = engineShaderPath("mesh.frag")}
    );
    const std::string vertex = opengl::translateToGlsl330(desc.vertex, rhi::ShaderStage::Vertex);
    CHECK(vertex.find("gl_Position.z = 2.0 * gl_Position.z - gl_Position.w") != std::string::npos);
}
#endif
