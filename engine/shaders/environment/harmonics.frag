#version 450

// Проекция неба из куба захвата в 9 коэффициентов гармоник: пиксель i цели
// 9 x 1 - коэффициент i. Направления - сетка равных телесных углов;
// гармоники второго порядка гладкие, поэтому куб читается с мелкого мипа.
#include <yqlora/harmonics.glsl>

layout(set = 2, binding = 0) uniform samplerCube source;

layout(location = 0) out vec4 fragColor;

const int kRows = 32;
const int kColumns = 64;
const float kPi = 3.14159265358979;
// Сторона грани мипа, с которого читается небо: мельче деталей гармоники
// всё равно не различат.
const float kReadSize = 16.0;

void main()
{
    int index = int(gl_FragCoord.x);
    float lod = max(log2(float(textureSize(source, 0).x) / kReadSize), 0.0);
    vec3 sum = vec3(0.0);
    for (int row = 0; row < kRows; ++row) {
        float y = 1.0 - 2.0 * (float(row) + 0.5) / float(kRows);
        float ring = sqrt(max(1.0 - y * y, 0.0));
        for (int column = 0; column < kColumns; ++column) {
            float angle = 2.0 * kPi * (float(column) + 0.5) / float(kColumns);
            vec3 direction = vec3(ring * cos(angle), y, ring * sin(angle));
            sum += textureLod(source, direction, lod).rgb * harmonicsBasis(index, direction);
        }
    }
    float solidAngle = 4.0 * kPi / float(kRows * kColumns);
    fragColor = vec4(sum * solidAngle, 1.0);
}
