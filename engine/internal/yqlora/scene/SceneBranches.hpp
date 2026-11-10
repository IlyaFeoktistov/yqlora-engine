#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/resource/ResourceId.hpp"
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

    // Корень экземпляра - обычными узлами с теми же ID, как после
    // "Распаковать": связи с префабом в ветке нет. Экземпляры, добавленные в
    // него, остаются экземплярами.
    static nlohmann::json writeUnpacked(const TypeRegistry& registry, const Node& node);

    // nullptr с сообщением в лог, если ветка не читается. Экземпляры
    // префабов в ветке разворачиваются из sources; без них остаются
    // узлами-заменами со своими записями.
    static std::unique_ptr<Node> read(
        const TypeRegistry& registry,
        const nlohmann::json& node,
        BranchIds ids,
        const PrefabSources* sources = nullptr
    );

    // Новый экземпляр префаба prefab (новый ID экземпляра) без переопределений:
    // так его создают игра и бросок в редакторе. nullptr с причиной в логе,
    // если prefab не развернулся (не префаб и не модель, нет файла, цикл).
    static std::unique_ptr<Node> instantiate(
        const TypeRegistry& registry,
        ResourceId prefab,
        const PrefabSources& sources
    );

    // Документ файла сцены или префаба текущей версии с корнем root: обратное
    // к rootOfFile.
    static nlohmann::json fileOf(nlohmann::json root);

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
