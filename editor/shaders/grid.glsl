// Блок сетки, общий для обеих стадий (GridUniforms в GridPass).
layout(set = 2, binding = 0, std140) uniform Grid {
    float cellSize;
    float fadeDistance;
} grid;
