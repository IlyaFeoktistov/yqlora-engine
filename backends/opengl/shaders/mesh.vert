#version 330 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;

layout(std140) uniform View {
    mat4 uViewProjection;
    vec3 uCameraPosition;
};

layout(std140) uniform Object {
    mat4 uModel;
    mat4 uNormalMatrix;
    vec4 uColor;
};

out vec3 vNormal;

void main()
{
    vNormal = mat3(uNormalMatrix) * aNormal;
    gl_Position = uViewProjection * uModel * vec4(aPosition, 1.0);
}
