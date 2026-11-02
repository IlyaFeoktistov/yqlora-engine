#version 450

layout(location = 0) out vec2 vUv;

// Один треугольник на весь экран по номеру вершины, без буфера.
void main()
{
    vUv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(vUv * 2.0 - 1.0, 0.0, 1.0);
}
