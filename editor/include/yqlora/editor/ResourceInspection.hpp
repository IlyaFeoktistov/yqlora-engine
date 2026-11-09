#pragma once

#include "yqlora/resource/ResourceCache.hpp"
#include "yqlora/resource/ResourceId.hpp"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <optional>
#include <string_view>

namespace yqlora::editor {

class EditorSession;

// Что инспектор показывает о любом выбранном файле ресурса. Своё у каждого
// вида - в его инспекции (ModelInspection.hpp, TextureInspection.hpp).
struct ResourceInspection {
    ResourceId id = 0;
    // Файл ресурса; у вложенного - файл, в котором он лежит.
    std::filesystem::path path;
    // Ключ вложенного ресурса без вида ("Bark"); пусто - ресурс сам файл.
    std::string nestedKey;
    // Вид ресурса (у файла - ResourceCache::kindOf); пусто - движок такие файлы не читает.
    std::string_view kind;
    // nullopt - файл не читается загрузчиком.
    std::optional<ResourceStatus> status;
};

// nullopt, если проекта нет или ресурса id в нём нет. Файл загружается, если
// ещё не был.
std::optional<ResourceInspection> inspectResource(EditorSession& session, ResourceId id);

// Ресурс id - свой файл проекта, правка пишется в него. Вложенный ресурс
// (материал модели) только читается: его значения - из файла модели.
bool isResourceFile(const EditorSession& session, ResourceId id);

// Настройки импорта файла id из .meta, объект; пустой, если их нет.
nlohmann::json importSettingsOf(EditorSession& session, ResourceId id);

// Записывает настройки импорта в .meta и перечитывает файл: новая ревизия,
// новые данные везде, где ресурс используется. Истории правок это не
// касается: файл и .meta - не сцена. false, если .meta не
// записалась.
bool applyImportSettings(EditorSession& session, ResourceId id, const nlohmann::json& settings);

} // namespace yqlora::editor
