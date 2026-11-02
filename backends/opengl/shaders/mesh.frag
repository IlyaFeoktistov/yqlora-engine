#version 330 core

in vec3 vNormal;

layout(std140) uniform Frame {
    vec3 uLightDirection;
};

layout(std140) uniform Object {
    mat4 uModel;
    mat4 uNormalMatrix;
    vec4 uColor;
};

out vec4 fragColor;

void main()
{
    // Рассеянный свет плюс постоянная подсветка, чтобы теневые стороны не были чёрными.
    const float kAmbient = 0.35;
    float diffuse = max(dot(normalize(vNormal), -uLightDirection), 0.0);
    float light = kAmbient + (1.0 - kAmbient) * diffuse;
    fragColor = vec4(uColor.rgb * light, uColor.a);
}
