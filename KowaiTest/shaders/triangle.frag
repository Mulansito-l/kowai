#version 450

layout(location = 0) in vec2 inTexCoord;
layout(location = 1) in vec3 inGouraudColor;

layout(location = 0) out vec4 outColor;

layout(set = 2, binding = 0) uniform sampler2D texSampler;

void main() {
    vec4 colorTextura = texture(texSampler, inTexCoord);
    
    // Si la textura tiene transparencia (ej. hojas de árboles, rejas) la respetamos
    if(colorTextura.a < 0.1) discard; 

    // El color final es la combinación del color interpolado por vértice (Gouraud) y la textura
    outColor = vec4(colorTextura.rgb * inGouraudColor, colorTextura.a);
}