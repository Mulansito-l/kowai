#include "KowaiEngine/scene.h"
#include "KowaiEngine/helpers.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// =====================================================
// CREATE SCENE
// =====================================================
KowaiScene* kowai_scene_create(const char* name) {
    KowaiScene* scene = (KowaiScene*)malloc(sizeof(KowaiScene));
    if (!scene) return NULL;

    memset(scene, 0, sizeof(KowaiScene));

    SDL_strlcpy(scene->name, name, sizeof(scene->name));

    scene->entity_count = 0;

    // ☀️ DIRECCIÓN DEL SOL: Una luz muy inclinada (atardecer/amanecer) o casi cenital muerta
    // Un sol que viene desde arriba-atrás de forma diagonal suave.
    glm_vec3_copy((vec3) { 0.2f, 0.8f, 0.4f }, scene->sun_direction);

    // 🎨 COLOR DEL SOL: Un tono pálido, frío o amarillento enfermo
    // En lugar de blanco puro, un blanco hueso deslavado o un gris amarillento sin vida.
    glm_vec3_copy((vec3) { 0.75f, 0.70f, 0.65f }, scene->sun_color);

    // 🌑 LUZ AMBIENTAL: El truco maestro de la PS2 (Oscuridad pero visible)
    // Si la dejas en (1.0, 1.0, 1.0), los objetos en la sombra se verán idénticos a los iluminados.
    // Necesitamos un azul grisáceo muy oscuro para simular penumbra en las zonas donde no pega el sol.
    glm_vec3_copy((vec3) { 0.12f, 0.14f, 0.18f }, scene->ambient_color);

    // 🌌 COLOR DEL CIELO (CLEAR COLOR): El vacío de fondo
    // Un azul marino/grisáceo profundamente oscuro y tétrico que se trague las siluetas de tus mallas.
    glm_vec3_copy((vec3) { 0.05f, 0.06f, 0.08f }, scene->sky_clear_color);

    for (int i = 0; i < MAX_ENTITIES_PER_SCENE; i++) {
        scene->entities[i].id = i;
        scene->entities[i].name[0] = '\0';

        scene->entities[i].component_count = 0;

        for (int j = 0; j < KOWAI_MAX_COMPONENTS_PER_ENTITY; j++) {
            scene->entities[i].components[j] = NULL;
        }

        scene->meshes[i].model = NULL;
        scene->meshes[i].is_visible = false;
    }

    return scene;
}

// =====================================================
// CLEAR SCENE
// =====================================================
void kowai_scene_clear(KowaiScene* scene) {
    if (!scene) return;

    memset(scene->entities, 0, sizeof(scene->entities));
    memset(scene->meshes, 0, sizeof(scene->meshes));

    scene->entity_count = 0;
}

void ensure_kscene_extension(const char* input, char* output, size_t size)
{
    const char* ext = strrchr(input, '.');

    if (ext && strcmp(ext, ".kscene") == 0)
    {
        SDL_strlcpy(output, input, size);
    }
    else
    {
        SDL_snprintf(output, size, "%s.kscene", input);
    }
}

bool kowai_scene_save(KowaiScene* scene, KowaiAssetBank* asset_bank, const char* filepath) {
    if (!scene || !filepath || !asset_bank) return false;

    SDL_IOStream* file = SDL_IOFromFile(filepath, "w");
    if (!file) return false;

    char buffer[512];

    // 1. Metadatos de la Escena
    SDL_snprintf(buffer, sizeof(buffer),
        "[Scene]\nname=%s\nsun_direction=%f,%f,%f\n\n",
        scene->name,
        scene->sun_direction[0], scene->sun_direction[1], scene->sun_direction[2]);
    SDL_WriteIO(file, buffer, SDL_strlen(buffer));

    // 2. Manifiesto de Recursos
    SDL_snprintf(buffer, sizeof(buffer), "[Manifest]\nasset_count=%d\n", asset_bank->asset_count);
    SDL_WriteIO(file, buffer, SDL_strlen(buffer));

    for (int i = 0; i < asset_bank->asset_count; i++) {
        KowaiAssetSlot* slot = &asset_bank->slots[i];
        const char* type_str = "none";
        if (slot->type == ASSET_TYPE_MODEL) type_str = "model";
        if (slot->type == ASSET_TYPE_TEXTURE) type_str = "texture";

        SDL_snprintf(buffer, sizeof(buffer), "asset_%d=%s,%s,%s\n", i, type_str, slot->id, slot->path);
        SDL_WriteIO(file, buffer, SDL_strlen(buffer));
    }
    SDL_WriteIO(file, "\n", 1);

    // 3. Serialización de Entidades y Componentes
    for (int i = 0; i < MAX_ENTITIES_PER_SCENE; i++) {
        KowaiEntity* e = &scene->entities[i];
        if (e->name[0] == '\0') continue;

        // Obtenemos los componentes de forma segura a través de tu API del ECS
        TransformComponent* t = (TransformComponent*)kowai_entity_get_component(e, COMPONENT_TRANSFORM);
        MeshRenderComponent* m = (MeshRenderComponent*)kowai_entity_get_component(e, COMPONENT_MESH_RENDER);
        LightComponent* l = (LightComponent*)kowai_entity_get_component(e, COMPONENT_LIGHT);

        // Si por alguna razón la entidad no tiene transform, usamos el del arreglo paralelo de respaldo
        if (!t) t = &scene->transforms[i];

        // Calcular el índice entero del padre en base a la dirección de memoria
        int parent_idx = -1;
        if (e->parent != NULL) {
            parent_idx = (int)(e->parent - scene->entities);
        }

        SDL_snprintf(buffer, sizeof(buffer),
            "[Entity_%d]\n"
            "name=%s\n"
            "parent_idx=%d\n"
            "position=%f,%f,%f\n"
            "rotation=%f,%f,%f\n"
            "scale=%f,%f,%f\n",
            i, e->name, parent_idx,
            t->position[0], t->position[1], t->position[2],
            t->rotation[0], t->rotation[1], t->rotation[2],
            t->scale[0], t->scale[1], t->scale[2]);
        SDL_WriteIO(file, buffer, SDL_strlen(buffer));

        // Guardar Componente de Luz (si existe) adaptado a tus nombres de campo
        if (l != NULL) {
            SDL_snprintf(buffer, sizeof(buffer),
                "has_light=1\n"
                "light_color=%f,%f,%f\n"
                "light_intensity=%f\n"
                "light_radius=%f\n",
                l->color[0], l->color[1], l->color[2],
                l->intensity, l->radius);
            SDL_WriteIO(file, buffer, SDL_strlen(buffer));
        }
        else {
            SDL_snprintf(buffer, sizeof(buffer), "has_light=0\n");
            SDL_WriteIO(file, buffer, SDL_strlen(buffer));
        }

        // Guardar Componente de Malla
        if (m && m->model) {
            const char* model_id = kowai_asset_bank_get_id_from_model(asset_bank, m->model);

            SDL_snprintf(buffer, sizeof(buffer),
                "has_mesh=1\nmodel_name=%s\nmesh_count=%d\n",
                model_id ? model_id : "unknown", m->model->mesh_count);
            SDL_WriteIO(file, buffer, SDL_strlen(buffer));

            for (Uint32 j = 0; j < m->model->mesh_count; j++) {
                KowaiMesh* mesh = &m->model->meshes[j];
                const char* tex_id = "none";

                if (mesh->texture) {
                    for (int k = 0; k < asset_bank->asset_count; k++) {
                        if (asset_bank->slots[k].type == ASSET_TYPE_TEXTURE &&
                            asset_bank->slots[k].asset.texture == (void*)mesh->texture) {
                            tex_id = asset_bank->slots[k].id;
                            break;
                        }
                    }
                }

                SDL_snprintf(buffer, sizeof(buffer), "mesh_tex_%d=%s\n", j, tex_id);
                SDL_WriteIO(file, buffer, SDL_strlen(buffer));
            }
        }
        else {
            SDL_snprintf(buffer, sizeof(buffer), "has_mesh=0\nmodel_name=none\n");
            SDL_WriteIO(file, buffer, SDL_strlen(buffer));
        }
    }

    SDL_strlcpy(scene->current_filepath, filepath, sizeof(scene->current_filepath));
    SDL_CloseIO(file);
    return true;
}

KowaiScene* kowai_scene_load(const char* filepath, KowaiAssetBank* asset_bank, SDL_GPUDevice* gpu_device) {
    if (!filepath || !asset_bank || !gpu_device) return NULL;

    SDL_IOStream* file = SDL_IOFromFile(filepath, "r");
    if (!file) return NULL;

    KowaiScene* scene = kowai_scene_create("LoadedScene");
    if (!scene) {
        SDL_CloseIO(file);
        return NULL;
    }

    // Tabla temporal para reconstruir el grafo al final
    int parent_indices[MAX_ENTITIES_PER_SCENE];
    for (int i = 0; i < MAX_ENTITIES_PER_SCENE; i++) parent_indices[i] = -1;

    char line[256];
    int current_entity_idx = -1;
    KowaiEntity* current_entity = NULL;
    bool in_entity = false;

    while (kowai_read_line(file, line, sizeof(line))) {
        line[strcspn(line, "\r\n")] = 0;

        if (strncmp(line, "[Scene]", 7) == 0 || strncmp(line, "[Manifest]", 10) == 0) continue;

        // --- Escena Metadatos ---
        if (strncmp(line, "name=", 5) == 0 && current_entity_idx == -1) {
            SDL_strlcpy(scene->name, line + 5, sizeof(scene->name));
            continue;
        }
        if (strncmp(line, "sun_direction=", 14) == 0) {
            sscanf(line + 14, "%f,%f,%f", &scene->sun_direction[0], &scene->sun_direction[1], &scene->sun_direction[2]);
            continue;
        }

        // --- Cargar Manifiesto ---
        if (strncmp(line, "asset_", 6) == 0) {
            char type[16], id[64], path[256];
            const char* data_start = SDL_strchr(line, '=');
            if (data_start && sscanf(data_start + 1, "%[^,],%[^,],%[^\n]", type, id, path) == 3) {
                path[strcspn(path, "\r\n")] = 0;
                if (strcmp(type, "model") == 0) {
                    kowai_asset_bank_register_gltf(gpu_device, asset_bank, id, path);
                }
                else if (strcmp(type, "texture") == 0) {
                    kowai_asset_bank_register_texture(gpu_device, asset_bank, id, path);
                }
            }
            continue;
        }

        // --- Entidades ---
        if (strncmp(line, "[Entity_", 8) == 0) {
            in_entity = true;
            sscanf(line + 8, "%d]", &current_entity_idx);
            if (current_entity_idx >= 0 && current_entity_idx < MAX_ENTITIES_PER_SCENE) {
                current_entity = &scene->entities[current_entity_idx];
                scene->entity_count++;

                // Limpieza explícita del árbol de herencias nativo
                current_entity->parent = NULL;
                current_entity->first_child = NULL;
                current_entity->next_sibling = NULL;
                current_entity->component_count = 0; // Limpia la lista del ECS antiguo
            }
            continue;
        }

        if (!in_entity) continue;

        if (strncmp(line, "name=", 5) == 0) SDL_strlcpy(current_entity->name, line + 5, sizeof(current_entity->name));

        if (strncmp(line, "parent_idx=", 11) == 0) {
            sscanf(line + 11, "%d", &parent_indices[current_entity_idx]);
        }

        if (strncmp(line, "position=", 9) == 0) sscanf(line + 9, "%f,%f,%f", &scene->transforms[current_entity_idx].position[0], &scene->transforms[current_entity_idx].position[1], &scene->transforms[current_entity_idx].position[2]);
        if (strncmp(line, "rotation=", 9) == 0) sscanf(line + 9, "%f,%f,%f", &scene->transforms[current_entity_idx].rotation[0], &scene->transforms[current_entity_idx].rotation[1], &scene->transforms[current_entity_idx].rotation[2]);
        if (strncmp(line, "scale=", 6) == 0) {

            TransformComponent* t =
                &scene->transforms[current_entity_idx];

            sscanf(
                line + 6,
                "%f,%f,%f",
                &t->scale[0],
                &t->scale[1],
                &t->scale[2]
            );

            kowai_entity_add_component(
                current_entity,
                COMPONENT_TRANSFORM,
                t
            );

            kowai_transform_update_matrix(
                current_entity
            );
        }

        // Parsear Componente de Luz usando campos corregidos
        if (strncmp(line, "has_light=1", 11) == 0) {
            kowai_entity_add_component(current_entity, COMPONENT_LIGHT, &scene->lights[current_entity_idx]);
        }
        if (strncmp(line, "light_color=", 12) == 0) {
            sscanf(line + 12, "%f,%f,%f", &scene->lights[current_entity_idx].color[0], &scene->lights[current_entity_idx].color[1], &scene->lights[current_entity_idx].color[2]);
        }
        if (strncmp(line, "light_intensity=", 16) == 0) {
            sscanf(line + 16, "%f", &scene->lights[current_entity_idx].intensity);
        }
        if (strncmp(line, "light_radius=", 13) == 0) { // Modificado de light_range a light_radius
            sscanf(line + 13, "%f", &scene->lights[current_entity_idx].radius);
        }

        // Parsear Componente de Malla
        if (strncmp(line, "has_mesh=1", 10) == 0) scene->meshes[current_entity_idx].is_visible = true;
        if (strncmp(line, "model_name=", 11) == 0) {
            const char* model_name = line + 11;
            if (strcmp(model_name, "none") != 0 && asset_bank) {
                KowaiModel* model = kowai_asset_bank_get_model(asset_bank, model_name);
                scene->meshes[current_entity_idx].model = model;
                kowai_entity_add_component(current_entity, COMPONENT_MESH_RENDER, &scene->meshes[current_entity_idx]);
            }
        }
        if (strncmp(line, "mesh_tex_", 9) == 0) {
            int mesh_idx = -1;
            char tex_id[64];
            if (sscanf(line + 9, "%d=%s", &mesh_idx, tex_id) == 2) {
                MeshRenderComponent* m = &scene->meshes[current_entity_idx];
                if (m->model && mesh_idx >= 0 && (Uint32)mesh_idx < m->model->mesh_count) {
                    KowaiMesh* mesh = &m->model->meshes[mesh_idx];
                    if (strcmp(tex_id, "none") != 0) {
                        void* cached_tex = kowai_asset_bank_get_texture(asset_bank, tex_id);
                        if (cached_tex) {
                            mesh->texture = (SDL_GPUTexture*)cached_tex;
                            if (mesh->sampler) SDL_ReleaseGPUSampler(gpu_device, mesh->sampler);
                            SDL_GPUSamplerCreateInfo samplerInfo = {
                                .min_filter = SDL_GPU_FILTER_NEAREST,
                                .mag_filter = SDL_GPU_FILTER_NEAREST,
                                .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
                                .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
                                .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
                                .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT
                            };
                            mesh->sampler = SDL_CreateGPUSampler(gpu_device, &samplerInfo);
                        }
                    }
                }
            }
        }
    }

    // =========================================================================
    // RECONSTRUCCIÓN DEL SCENE GRAPH (JERARQUÍA COMPLETA)
    // =========================================================================
    for (int i = 0; i < MAX_ENTITIES_PER_SCENE; i++) {
        int p_idx = parent_indices[i];
        if (p_idx >= 0 && p_idx < MAX_ENTITIES_PER_SCENE) {
            KowaiEntity* hijo = &scene->entities[i];
            KowaiEntity* padre = &scene->entities[p_idx];

            // Re-vincula de forma nativa los punteros de árbol
            kowai_entity_set_parent(hijo, padre);
        }
    }

    SDL_strlcpy(scene->current_filepath, filepath, sizeof(scene->current_filepath));
    SDL_CloseIO(file);
    SDL_Log("KowaiEngine: Escena cargada correctamente con Jerarquía nativa y Luces vinculadas al ECS.");
    return scene;
}

// =====================================================
// DESTROY SCENE
// =====================================================
void kowai_scene_destroy(KowaiScene* scene) {
    if (!scene) return;
    free(scene);
}

// =====================================================
// CREATE ENTITY
// =====================================================
KowaiEntity* kowai_scene_create_entity(KowaiScene* scene, const char* name) {
    if (!scene || !name) return NULL;

    for (int i = 0; i < MAX_ENTITIES_PER_SCENE; i++) {
        if (scene->entities[i].name[0] == '\0') {

            // 1. Inicializar la entidad básica
            kowai_entity_init(&scene->entities[i], i, name);

            // 🟢 NUEVO: Asegurar que los punteros del Scene Graph nazcan limpios
            scene->entities[i].parent = NULL;
            scene->entities[i].first_child = NULL;
            scene->entities[i].next_sibling = NULL;

            // 2. Configurar valores TRS iniciales del transform
            TransformComponent* t = &scene->transforms[i];
            glm_vec3_copy((vec3) { 0.0f, 0.0f, 0.0f }, t->position);
            glm_vec3_copy((vec3) { 0.0f, 0.0f, 0.0f }, t->rotation);
            glm_vec3_copy((vec3) { 1.0f, 1.0f, 1.0f }, t->scale);

            // 🟢 NUEVO: Limpiar las matrices para que no tengan basura de la RAM
            glm_mat4_identity(t->local_matrix);
            glm_mat4_identity(t->global_matrix);
            glm_mat3_identity(t->normal_matrix);

            // 3. Acoplar el componente a la entidad primero
            kowai_entity_add_component(&scene->entities[i], COMPONENT_TRANSFORM, t);

            // 4. 🟢 CRÍTICO: Actualizar la matriz pasándole la ENTIDAD entera
            // Esto calculará local_matrix, global_matrix y normal_matrix correctamente
            kowai_transform_update_matrix(&scene->entities[i]);

            scene->entity_count++;

            return &scene->entities[i];
        }
    }

    return NULL;
}

// =====================================================================
// FUNCIÓN AUXILIAR RECURSIVA: Destruye una entidad y todos sus hijos
// =====================================================================
static void kowai_entity_destroy_recursive(KowaiScene* scene, KowaiEntity* entity) {
    if (!entity || entity->name[0] == '\0') return;

    // 1. Primero, destruir a todos sus hijos en cascada de forma recursiva
    KowaiEntity* hijo_actual = entity->first_child;
    while (hijo_actual != NULL) {
        // Guardamos el puntero al siguiente hermano ANTES de destruir al actual
        KowaiEntity* siguiente_hermano = hijo_actual->next_sibling;

        kowai_entity_destroy_recursive(scene, hijo_actual);

        hijo_actual = siguiente_hermano;
    }

    // 2. Calcular el índice (ID) real de esta entidad en el arreglo plano de la escena
    uint32_t id = (uint32_t)(entity - scene->entities);

    // 3. Limpiar los componentes y la metadata de la entidad
    entity->name[0] = '\0';
    entity->component_count = 0;
    memset(entity->components, 0, sizeof(entity->components));
    memset(entity->component_types, 0, sizeof(entity->component_types));

    // 4. Limpiar los componentes de los arreglos paralelos de la escena
    scene->meshes[id].model = NULL;
    scene->meshes[id].is_visible = false;

    // Si tienes un arreglo paralelo para las luces, lo limpias aquí también:
    // memset(&scene->lights[id], 0, sizeof(LightComponent));

    // 5. Resetear sus propios punteros de jerarquía por seguridad
    entity->parent = NULL;
    entity->first_child = NULL;
    entity->next_sibling = NULL;

    // 6. Decrementar el contador global de la escena
    scene->entity_count--;
}

// =====================================================================
// DESTROY ENTITY (Punto de entrada principal)
// =====================================================================
bool kowai_scene_destroy_entity(KowaiScene* scene, uint32_t id) {
    if (!scene || id >= MAX_ENTITIES_PER_SCENE) return false;

    KowaiEntity* e = &scene->entities[id];
    if (e->name[0] == '\0') return false;

    // 🟢 PASO CLAVE: Desenlazar la entidad de su Padre y sus Hermanos antes de borrarla
    if (e->parent != NULL) {
        KowaiEntity* padre = e->parent;

        if (padre->first_child == e) {
            // Caso A: Es el primer hijo, el padre ahora apunta al hermano de esta entidad
            padre->first_child = e->next_sibling;
        }
        else {
            // Caso B: No es el primero, hay que buscar cuál de sus hermanos apuntaba a él
            KowaiEntity* hermano = padre->first_child;
            while (hermano != NULL && hermano->next_sibling != e) {
                hermano = hermano->next_sibling;
            }
            if (hermano != NULL) {
                // El hermano anterior ahora salta a esta entidad y apunta al siguiente
                hermano->next_sibling = e->next_sibling;
            }
        }
    }

    // Iniciar la destrucción en cadena (de forma recursiva destruirá componentes e hijos)
    kowai_entity_destroy_recursive(scene, e);

    return true;
}