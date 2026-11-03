#pragma once

#include "yqlora/resource/ResourceId.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace yqlora::editor {

class EditorSession;

struct MeshSummary {
    ResourceId id = 0;
    // Ключ без вида: "Door", "Door#1".
    std::string label;
    std::size_t vertices = 0;
    std::size_t triangles = 0;
    std::size_t submeshes = 0;
};

// Настройки импорта модели, которые редактор показывает и меняет.
struct ModelImport {
    float scale = 1.0f;

    bool operator==(const ModelImport&) const = default;
};

// Что инспектор модели показывает: её меши в порядке файла, число узлов и
// настройки импорта, как они записаны в .meta.
struct ModelInspection {
    std::vector<MeshSummary> meshes;
    std::size_t nodes = 0;
    ModelImport import;
};

// nullopt, если id - не файл модели проекта или модель не загрузилась.
std::optional<ModelInspection> inspectModel(EditorSession& session, ResourceId id);

// См. applyImportSettings в ResourceInspection.hpp.
bool applyModelImport(EditorSession& session, ResourceId id, const ModelImport& settings);

} // namespace yqlora::editor
