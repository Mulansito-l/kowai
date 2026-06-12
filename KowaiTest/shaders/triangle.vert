#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec2 outTexCoord;
layout(location = 1) out vec3 outGouraudColor;

layout(set = 1, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

void main() {
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
    outTexCoord = inTexCoord;

    vec3 normalMundo = normalize(mat3(ubo.model) * inNormal);
    vec3 direccionLuz = normalize(vec3(0.5, 1.0, 0.3));

    float difusa = max(dot(normalMundo, direccionLuz), 0.0);
    float ambiental = 0.25;
    float brilloTotal = difusa + ambiental;

    outGouraudColor = vec3(min(brilloTotal, 1.0));
}