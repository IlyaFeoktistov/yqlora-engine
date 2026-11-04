// Грани кубической текстуры: направление из грани и точки на ней по таблице,
// общей для OpenGL, Vulkan и D3D (+X, -X, +Y, -Y, +Z, -Z; s, t от 0 до 1,
// t = 0 - первая строка грани). Так грань рисуется пиксель в пиксель с тем,
// как её потом прочтёт samplerCube, без камер и переворотов.
#ifndef YQLORA_CUBE_GLSL
#define YQLORA_CUBE_GLSL

vec3 cubeDirection(int face, vec2 st)
{
    vec2 c = st * 2.0 - 1.0;
    vec3 direction;
    switch (face) {
    case 0: direction = vec3(1.0, -c.y, -c.x); break;
    case 1: direction = vec3(-1.0, -c.y, c.x); break;
    case 2: direction = vec3(c.x, 1.0, c.y); break;
    case 3: direction = vec3(c.x, -1.0, -c.y); break;
    case 4: direction = vec3(c.x, -c.y, 1.0); break;
    default: direction = vec3(-c.x, -c.y, -1.0); break;
    }

    return normalize(direction);
}

#endif
