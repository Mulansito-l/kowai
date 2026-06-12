#ifndef KOWAI_ENTITY_H
#define KOWAI_ENTITY_H

#include <KowaiEngine/cglm/cglm.h>
#include <stdint.h>
#include <stdbool.h>

#define KOWAI_MAX_COMPONENTS_PER_ENTITY 8

// Tipos de componentes soportados por el motor
typedef enum {
    COMPONENT_TRANSFORM,
    COMPONENT_MESH_RENDER
} KowaiComponentType;

// --- COMPONENTE: TRANSFORM ---
typedef struct {
    vec3 position;
    vec3 rotation;
    vec3 scale;
    mat4 matrix;
} TransformComponent;

// --- COMPONENTE: MESH RENDER ---
typedef struct {
    struct KowaiModel* model; // Guardamos el puntero al glTF cargado
    bool is_visible;
} MeshRenderComponent;

// --- ESTRUCTURA DE LA ENTIDAD ---
typedef struct KowaiEntity {
    uint32_t id;
    char name[64]; // En lugar de const char* name;

    // Almacenamiento compacto de componentes adjuntos
    void* components[KOWAI_MAX_COMPONENTS_PER_ENTITY];
    KowaiComponentType component_types[KOWAI_MAX_COMPONENTS_PER_ENTITY];
    uint32_t component_count;
} KowaiEntity;

// --- FUNCIONES DE GESTIÓN ---
void kowai_entity_init(KowaiEntity* entity, uint32_t id, const char* name);
void kowai_entity_add_component(KowaiEntity* entity, KowaiComponentType type, void* component_data);
void* kowai_entity_get_component(const KowaiEntity* entity, KowaiComponentType type);
void kowai_entity_remove_component(KowaiEntity* entity, KowaiComponentType type);

// Funciones específicas para actualizar la matemática del Transform
void kowai_transform_update_matrix(TransformComponent* transform);

#endif // KOWAI_ENTITY_H