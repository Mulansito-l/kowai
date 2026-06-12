#ifndef KOWAI_ENTITY_H
#define KOWAI_ENTITY_H

#include <KowaiEngine/cglm/cglm.h>
#include <SDL3/SDL_gpu.h>
#include <stdint.h>
#include <stdbool.h>

struct KowaiModel;

#define KOWAI_MAX_COMPONENTS_PER_ENTITY 8
#define MAX_LIGHTS_PER_OBJECT 4

// Tipos de componentes soportados por el motor
typedef enum {
    COMPONENT_TRANSFORM,
    COMPONENT_MESH_RENDER,
    COMPONENT_LIGHT
} KowaiComponentType;

// --- COMPONENTE: TRANSFORM ---
typedef struct {
    vec3 position;
    vec3 rotation;
    vec3 scale;
    mat4 local_matrix;   // Reemplaza o renombra tu 'matrix' actual
    mat4 global_matrix;  // NUEVO: Posición final en el mundo
    mat3 normal_matrix;  // Se mantiene igual
} TransformComponent;

// --- COMPONENTE: MESH RENDER ---
typedef struct {
    struct KowaiModel* model;
    bool is_visible;
} MeshRenderComponent;

typedef struct {
    vec3 color;     // RGB de la luz
    float intensity;   // Multiplicador de brillo
    float radius;      // Qué tanto estira la luz antes de apagarse a cero
} LightComponent;

// --- ESTRUCTURA DE LA ENTIDAD ---
typedef struct KowaiEntity {
    uint32_t id;
    char name[64]; // En lugar de const char* name;

    // Almacenamiento compacto de componentes adjuntos
    void* components[KOWAI_MAX_COMPONENTS_PER_ENTITY];
    KowaiComponentType component_types[KOWAI_MAX_COMPONENTS_PER_ENTITY];
    uint32_t component_count;

    struct KowaiEntity* parent;      // Quién es mi padre (NULL si es raíz)
    struct KowaiEntity* first_child; // Mi primer hijo
    struct KowaiEntity* next_sibling;// Mi hermano siguiente (mismo padre)
} KowaiEntity;

// --- FUNCIONES DE GESTIÓN ---
void kowai_entity_init(KowaiEntity* entity, uint32_t id, const char* name);
void kowai_entity_add_component(KowaiEntity* entity, KowaiComponentType type, void* component_data);
void* kowai_entity_get_component(const KowaiEntity* entity, KowaiComponentType type);
void kowai_entity_remove_component(KowaiEntity* entity, KowaiComponentType type);
void kowai_entity_set_parent(KowaiEntity* child, KowaiEntity* new_parent);

// Funciones específicas para actualizar la matemática del Transform
void kowai_transform_update_matrix(TransformComponent* transform);


#endif // KOWAI_ENTITY_H