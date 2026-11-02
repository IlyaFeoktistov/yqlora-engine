#pragma once

#include "yqlora/core/Export.hpp"
#include "yqlora/math/Math.hpp"

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace yqlora {

struct Vertex {
    Vec3 position;
    Vec3 normal;
    Vec2 uv0{0.0f};
    // Как TANGENT в glTF: xyz - направление роста u, w - знак бинормали
    // (cross(normal, tangent.xyz) * w смотрит вверх по картинке), поэтому
    // работают и зеркальные развёртки.
    Vec4 tangent{1.0f, 0.0f, 0.0f, 1.0f};
};

// Часть меша со своим материалом. Индекс подмеша в MeshData::submeshes -
// слот материала у MeshRenderer, поэтому порядок подмешей постоянный.
struct SubMesh {
    std::uint32_t firstIndex = 0;
    std::uint32_t indexCount = 0;
    // Материал исходного файла (индекс материала в glTF); nullopt - без
    // материала. По нему импорт материалов найдёт, что куда положить.
    std::optional<int> sourceMaterial;
};

struct Bounds {
    Vec3 min{0.0f};
    Vec3 max{0.0f};
};

// Геометрия на стороне CPU, откуда бы она ни пришла: встроенная фигура,
// файл модели, код игры. Треугольники перечислены против часовой стрелки,
// если смотреть снаружи: задние грани отсекаются.
struct MeshData {
    static constexpr std::string_view kResourceKind = "mesh";

    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    // Покрывают все индексы по порядку; хотя бы один, если индексы есть.
    std::vector<SubMesh> submeshes;
    Bounds bounds;
};

// Границы по вершинам; у меша без вершин - нулевые.
YQLORA_API Bounds computeBounds(const std::vector<Vertex>& vertices);

// Один подмеш на все индексы и границы: так заканчивают меш с одним материалом.
YQLORA_API void finishSingleMaterial(MeshData& mesh);

} // namespace yqlora
