#include "KowaiEngine/entity.h"
#include <stddef.h>

void kowai_entity_init(KowaiEntity* entity, uint32_t id, const char* name) {
    if (!entity) return;
    entity->id = id;
    SDL_strlcpy(entity->name, name, sizeof(entity->name));
    entity->component_count = 0;

    for (int i = 0; i < KOWAI_MAX_COMPONENTS_PER_ENTITY; i++) {
        entity->components[i] = NULL;
    }
}

void kowai_entity_add_component(KowaiEntity* entity, KowaiComponentType type, void* component_data) {
    if (!entity || !component_data) return;
    if (entity->component_count >= KOWAI_MAX_COMPONENTS_PER_ENTITY) return;

    // Verificar que no se añada el mismo componente dos veces
    if (kowai_entity_get_component(entity, type) != NULL) return;

    uint32_t idx = entity->component_count;
    entity->components[idx] = component_data;
    entity->component_types[idx] = type;
    entity->component_count++;
}

void* kowai_entity_get_component(const KowaiEntity* entity, KowaiComponentType type) {
    if (!entity) return NULL;

    for (uint32_t i = 0; i < entity->component_count; i++) {
        if (entity->component_types[i] == type) {
            return entity->components[i];
        }
    }
    return NULL;
}

void kowai_transform_update_matrix(TransformComponent* transform) {
    if (!transform) return;

    // Construir la matriz TRS (Translate, Rotate, Scale) de toda la vida
    glm_mat4_identity(transform->matrix);
    glm_translate(transform->matrix, transform->position);

    // Rotación en los 3 ejes
    glm_rotate(transform->matrix, transform->rotation[0], (vec3) { 1.0f, 0.0f, 0.0f });
    glm_rotate(transform->matrix, transform->rotation[1], (vec3) { 0.0f, 1.0f, 0.0f });
    glm_rotate(transform->matrix, transform->rotation[2], (vec3) { 0.0f, 0.0f, 1.0f });

    glm_scale(transform->matrix, transform->scale);
}

void kowai_entity_remove_component(KowaiEntity* entity, KowaiComponentType type) {
    if (!entity || entity->component_count == 0) return;

    int target_idx = -1;

    // 1. Buscar en qué índice del array está el componente
    for (uint32_t i = 0; i < entity->component_count; i++) {
        if (entity->component_types[i] == type) {
            target_idx = (int)i;
            break;
        }
    }

    // Si no lo encontramos, salimos
    if (target_idx == -1) return;

    // 2. Desplazar los componentes siguientes hacia atrás para mantener el array contiguo
    for (uint32_t i = target_idx; i < entity->component_count - 1; i++) {
        entity->components[i] = entity->components[i + 1];
        entity->component_types[i] = entity->component_types[i + 1];
    }

    // 3. Limpiar el último slot que quedó duplicado al final y restar el contador
    uint32_t ultimo_idx = entity->component_count - 1;
    entity->components[ultimo_idx] = NULL;
    entity->component_count--;
}