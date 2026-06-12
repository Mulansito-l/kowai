#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec2 outTexCoord;
layout(location = 1) out vec3 outGouraudColor;

struct PointLight {
    vec4 positionAndActive; // .xyz = position, .w = isActive
    vec4 colorAndRadius;    // .xyz = color,    .w = radius
};

#define MAX_LIGHTS 4

layout(set = 1, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    mat4 normalMatrix;
    vec4 sunDirection;
    vec4 sunColor;
    vec4 ambientColor;
    PointLight lights[MAX_LIGHTS];
} ubo;

void main() {
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
    outTexCoord = inTexCoord;

    vec3 normalMundo = normalize(mat3(ubo.normalMatrix) * inNormal);
    vec3 posicionMundo = (ubo.model * vec4(inPosition, 1.0)).xyz;

    // Luz del Sol y Ambiente base
    vec3 direccionSol = normalize(ubo.sunDirection.xyz);
    float difusaSol = max(dot(normalMundo, direccionSol), 0.0);
    vec3 iluminacionAcumulada = (difusaSol * ubo.sunColor.xyz) + ubo.ambientColor.xyz;

    // Procesar las 4 luces dinámicas fijas
    for (int i = 0; i < MAX_LIGHTS; i++) {
        if (ubo.lights[i].positionAndActive.w > 0.5) { // Si isActive == 1.0
            vec3 luzPos = ubo.lights[i].positionAndActive.xyz;
            vec3 vectorLuz = luzPos - posicionMundo;
            float distancia = length(vectorLuz);
            float radius = ubo.lights[i].colorAndRadius.w;

            if (distancia < radius) {
                vec3 dirLuz = normalize(vectorLuz);
                float difusaPuntual = max(dot(normalMundo, dirLuz), 0.0);

                // 🟢 CORREGIDO: Atenuación con caída curva más suave (Look retro mejorado)
                float factorLineal = 1.0 - (distancia / radius);
            
                // Elevarlo al cuadrado (o al cubo) genera una curva de desvanecimiento suave 
                // que evita que el degradado muera tan rápido a mitad del camino.
                float atenuacion = factorLineal * factorLineal; 

                vec3 luzColor = ubo.lights[i].colorAndRadius.xyz;
            
                // NOTA: Si notas que le falta fuerza por la curva, puedes multiplicar aquí por tu intensidad
                iluminacionAcumulada += (difusaPuntual * luzColor) * atenuacion;
            }
        }
    }

    outGouraudColor = min(iluminacionAcumulada, vec3(1.0));
}