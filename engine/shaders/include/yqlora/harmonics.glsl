// Сферические гармоники второго порядка (9 коэффициентов): ими движок
// хранит окружающий свет. Проекцию (sky.glsl) и чтение (lighting.glsl)
// задают одни и те же базисные функции отсюда.
#ifndef YQLORA_HARMONICS_GLSL
#define YQLORA_HARMONICS_GLSL

#define YQLORA_HARMONICS_COUNT 9

// Вещественный базис Y_lm в направлении d (единичном), по порядку
// l = 0; l = 1: m = -1, 0, 1; l = 2: m = -2 .. 2.
float harmonicsBasis(int index, vec3 d)
{
    switch (index) {
    case 0: return 0.282095;
    case 1: return 0.488603 * d.y;
    case 2: return 0.488603 * d.z;
    case 3: return 0.488603 * d.x;
    case 4: return 1.092548 * d.x * d.y;
    case 5: return 1.092548 * d.y * d.z;
    case 6: return 0.315392 * (3.0 * d.z * d.z - 1.0);
    case 7: return 1.092548 * d.x * d.z;
    default: return 0.546274 * (d.x * d.x - d.y * d.y);
    }
}

// Свёртка с косинусом (Ramamoorthi и Hanrahan, 2001), делённая на пи:
// A_l / pi для l = 0, 1, 2. Постоянное небо яркости L даёт ровно L.
float harmonicsCosineBand(int index)
{
    return index == 0 ? 1.0 : (index < 4 ? 2.0 / 3.0 : 0.25);
}

#endif
