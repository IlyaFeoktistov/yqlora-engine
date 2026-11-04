#include "yqlora/editor/EditorSettings.hpp"

#include "yqlora/core/JsonFile.hpp"
#include "yqlora/core/Utf8Path.hpp"

#include <nlohmann/json.hpp>

#include <type_traits>

namespace yqlora::editor {

namespace {

constexpr const char* kCameraPreviewKey = "cameraPreview";
constexpr const char* kViewportSkyKey = "viewportSky";
constexpr const char* kReparentKeepWorldKey = "reparentKeepWorld";
constexpr const char* kSnapKey = "snap";
constexpr const char* kSnapGridKey = "grid";
constexpr const char* kMoveStepKey = "moveStep";
constexpr const char* kRotateStepKey = "rotateStepDegrees";
constexpr const char* kScaleStepKey = "scaleStep";
constexpr const char* kAlignToSurfaceKey = "alignToSurface";

// Поля привязки с их ключами: одно место и для чтения, и для записи.
template <typename Snap, typename Visitor>
void forEachSnapField(Snap& snap, Visitor&& visit)
{
    visit(kSnapGridKey, snap.grid);
    visit(kMoveStepKey, snap.moveStep);
    visit(kRotateStepKey, snap.rotateStepDegrees);
    visit(kScaleStepKey, snap.scaleStep);
    visit(kAlignToSurfaceKey, snap.alignToSurface);
}
constexpr const char* kToolchainKey = "toolchain";
constexpr const char* kCmakeKey = "cmake";
constexpr const char* kMakeProgramKey = "makeProgram";
constexpr const char* kCCompilerKey = "cCompiler";
constexpr const char* kCxxCompilerKey = "cxxCompiler";

// Поля переопределений с их ключами: одно место и для чтения, и для записи.
template <typename Overrides, typename Visitor>
void forEachOverride(Overrides& overrides, Visitor&& visit)
{
    visit(kCmakeKey, overrides.cmake);
    visit(kMakeProgramKey, overrides.makeProgram);
    visit(kCCompilerKey, overrides.cCompiler);
    visit(kCxxCompilerKey, overrides.cxxCompiler);
}

} // namespace

EditorSettings::EditorSettings(std::filesystem::path file)
    : m_file(std::move(file))
{
}

void EditorSettings::load()
{
    const auto json = readJsonFile(m_file);
    if (!json || !json->is_object()) {
        return;
    }
    if (const auto it = json->find(kCameraPreviewKey); it != json->end() && it->is_boolean()) {
        cameraPreview = it->get<bool>();
    }
    if (const auto it = json->find(kViewportSkyKey); it != json->end() && it->is_boolean()) {
        viewportSky = it->get<bool>();
    }
    if (const auto it = json->find(kReparentKeepWorldKey); it != json->end() && it->is_boolean()) {
        reparentKeepWorld = it->get<bool>() ? KeepWorld::Yes : KeepWorld::No;
    }
    if (const auto it = json->find(kSnapKey); it != json->end() && it->is_object()) {
        forEachSnapField(snap, [&](const char* key, auto& field) {
            using Field = std::decay_t<decltype(field)>;
            const auto value = it->find(key);
            if (value == it->end()) {
                return;
            }
            if constexpr (std::is_same_v<Field, bool>) {
                if (value->is_boolean()) {
                    field = value->template get<bool>();
                }
            } else if (value->is_number() && value->template get<Field>() > 0) {
                // Шаг 0 или меньше привязку бы сломал: остаётся по умолчанию.
                field = value->template get<Field>();
            }
        });
    }
    if (const auto it = json->find(kToolchainKey); it != json->end() && it->is_object()) {
        forEachOverride(toolchain, [&](const char* key, std::filesystem::path& path) {
            if (const auto value = it->find(key); value != it->end() && value->is_string()) {
                path = fromUtf8(value->get<std::string>());
            }
        });
    }
}

bool EditorSettings::save() const
{
    nlohmann::json overrides = nlohmann::json::object();
    forEachOverride(toolchain, [&](const char* key, const std::filesystem::path& path) {
        if (!path.empty()) {
            overrides[key] = toUtf8(path);
        }
    });

    nlohmann::json snapJson = nlohmann::json::object();
    forEachSnapField(snap, [&](const char* key, const auto& field) {
        snapJson[key] = field;
    });
    nlohmann::json json{
        {kCameraPreviewKey, cameraPreview},
        {kViewportSkyKey, viewportSky},
        {kSnapKey, snapJson},
        {kToolchainKey, overrides},
    };
    if (reparentKeepWorld) {
        json[kReparentKeepWorldKey] = *reparentKeepWorld == KeepWorld::Yes;
    }

    return writeJsonFile(m_file, json);
}

} // namespace yqlora::editor
