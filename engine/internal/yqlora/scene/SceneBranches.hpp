#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/resource/ResourceKinds.hpp"

#include <nlohmann/json.hpp>

#include <memory>
#include <optional>
#include <string_view>

namespace yqlora {

class Node;
class TypeRegistry;
struct PrefabSources;

// Брать ли ID узлов и компонентов из ветки или дать новые.
enum class BranchIds {
    FromFile,
    // Новые ID: копия ветки рядом с оригиналом.
    Fresh,
};

// Ветка узлов в формате файла сцены (как "root" в .yscene, без версии):
// прототип префаба, который хранит кэш ресурсов. Читается и пишется тем же
// кодом, что сцена (SceneSerializer), поэтому формат у них один.
struct YQLORA_API SceneBranches {
    static nlohmann::json write(const TypeRegistry& registry, const Node& node);

    // nullptr с сообщением в лог, если ветка не читается. Экземпляры
    // префабов в ветке разворачиваются из sources; без них остаются
    // узлами-заменами со своими записями.
    static std::unique_ptr<Node> read(
        const TypeRegistry& registry,
        const nlohmann::json& node,
        BranchIds ids,
        const PrefabSources* sources = nullptr
    );

    // Корень дерева из документа файла сцены или префаба, переведённого на
    // текущую версию формата; null-корень у пустой сцены. nullopt - документ
    // не объект или версия не читается (причина в логе).
    static std::optional<nlohmann::json> rootOfFile(nlohmann::json file);
};

// Разобранный файл сцены или префаба в кэше ресурсов: корень в формате файла.
// Узлы из него строит SceneBranches::read, когда префаб ставят в сцену.
struct SceneTree {
    nlohmann::json root;
};

struct SceneData : SceneTree {
    static constexpr std::string_view kResourceKind = kinds::kScene;
};

struct PrefabData : SceneTree {
    static constexpr std::string_view kResourceKind = kinds::kPrefab;
};

} // namespace yqlora
