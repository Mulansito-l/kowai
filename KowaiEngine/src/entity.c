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

void kowai_entity_set_parent(KowaiEntity* child, KowaiEntity* new_parent) {
    if (!child) return;

    // 1. SI YA TENÍA UN PADRE PREVIO, TENEMOS QUE DESACOPLARLO PRIMERO
    // (Para no romper la lista enlazada del viejo padre)
    if (child->parent) {
        KowaiEntity* old_parent = child->parent;

        if (old_parent->first_child == child) {
            // Si era el primer hijo, el nuevo primer hijo es su hermano siguiente
            old_parent->first_child = child->next_sibling;
        }
        else {
            // Si estaba en medio, hay que buscar al hermano anterior para saltarse a 'child'
            KowaiEntity* prev_sibling = old_parent->first_child;
            while (prev_sibling && prev_sibling->next_sibling != child) {
                prev_sibling = prev_sibling->next_sibling;
            }
            if (prev_sibling) {
                prev_sibling->next_sibling = child->next_sibling;
            }
        }
    }

    // 2. ASIGNAR EL NUEVO PADRE
    child->parent = new_parent;

    // 3. ENLAZARLO EN LA LISTA ENLAZADA DEL NUEVO PADRE
    if (new_parent != NULL) {
        // El hermano siguiente del nuevo hijo será el que antes era el primer hijo
        child->next_sibling = new_parent->first_child;

        // Ahora este objeto se convierte en el nuevo primer hijo oficial del padre
        new_parent->first_child = child;
    }
    else {
        // Si el nuevo padre es NULL, se convierte en una entidad raíz del mundo
        child->next_sibling = NULL;
    }

    // 4. 🟢 CRÍTICO: Forzar recálculo de matrices para que herede la nueva posición global
    kowai_transform_update_matrix(child);
}

void kowai_transform_update_matrix(KowaiEntity* entity) {
    if (!entity) return;

    TransformComponent* transform = (TransformComponent*)kowai_entity_get_component(entity, COMPONENT_TRANSFORM);
    if (!transform) return;

    // =====================================================================
    // 1. Construir la matriz TRS LOCAL (Exactamente tu código actual)
    // =====================================================================
    glm_mat4_identity(transform->local_matrix);
    glm_translate(transform->local_matrix, transform->position);

    glm_rotate(transform->local_matrix, transform->rotation[0], (vec3) { 1.0f, 0.0f, 0.0f });
    glm_rotate(transform->local_matrix, transform->rotation[1], (vec3) { 0.0f, 1.0f, 0.0f });
    glm_rotate(transform->local_matrix, transform->rotation[2], (vec3) { 0.0f, 0.0f, 1.0f });

    glm_scale(transform->local_matrix, transform->scale);

    // =====================================================================
    // 2. 🟢 NUEVO: Calcular la MATRIZ GLOBAL combinando con el Padre
    // =====================================================================
    if (entity->parent) {
        TransformComponent* parent_transform = (TransformComponent*)kowai_entity_get_component(entity->parent, COMPONENT_TRANSFORM);
        if (parent_transform) {
            // MatrizGlobal = MatrizPadreGlobal * MatrizLocal
            glm_mat4_mul(parent_transform->global_matrix, transform->local_matrix, transform->global_matrix);
        }
        else {
            // Si el padre no tiene transform (raro), la local es la global
            glm_mat4_copy(transform->local_matrix, transform->global_matrix);
        }
    }
    else {
        // Si no tiene padre (es raíz), la global es idéntica a la local
        glm_mat4_copy(transform->local_matrix, transform->global_matrix);
    }

    // =====================================================================
    // 3. 🟢 NUEVO: Extraer la Matriz de Normales desde la MATRIZ GLOBAL
    // =====================================================================
    mat3 upper_left;
    glm_vec3_copy(transform->global_matrix[0], upper_left[0]);
    glm_vec3_copy(transform->global_matrix[1], upper_left[1]);
    glm_vec3_copy(transform->global_matrix[2], upper_left[2]);

    mat3 inv_matrix;
    glm_mat3_inv(upper_left, inv_matrix);
    glm_mat3_transpose_to(inv_matrix, transform->normal_matrix);

    // =====================================================================
    // 4. 🟢 CRÍTICO: Propagar el movimiento a los hijos de forma recursiva
    // =====================================================================
    KowaiEntity* child = entity->first_child;
    while (child != NULL) {
        // Llamada recursiva: si el padre se mueve, el hijo se recalcula automáticamente
        kowai_transform_update_matrix(child);
        child = child->next_sibling;
    }
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