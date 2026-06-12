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

    glm_vec3_copy((vec3) { 0.5f, 1.0f, 0.3f }, scene->sun_direction);

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

// =====================================================
// SAVE SCENE
// =====================================================
bool kowai_scene_save(KowaiScene* scene, KowaiAssetBank* asset_bank, const char* filepath) {
    if (!scene || !filepath) return false;

    SDL_IOStream* file = SDL_IOFromFile(filepath, "w");
    if (!file) return false;

    char buffer[512];

    SDL_snprintf(buffer, sizeof(buffer),
        "[Scene]\nname=%s\nsun_direction=%f,%f,%f\n\n",
        scene->name,
        scene->sun_direction[0],
        scene->sun_direction[1],
        scene->sun_direction[2]);

    SDL_WriteIO(file, buffer, SDL_strlen(buffer));

    for (int i = 0; i < MAX_ENTITIES_PER_SCENE; i++) {
        KowaiEntity* e = &scene->entities[i];

        if (e->name[0] == '\0') continue;

        TransformComponent* t = &scene->transforms[i];
        MeshRenderComponent* m = &scene->meshes[i];

        SDL_snprintf(buffer, sizeof(buffer),
            "[Entity_%d]\n"
            "name=%s\n"
            "position=%f,%f,%f\n"
            "rotation=%f,%f,%f\n"
            "scale=%f,%f,%f\n",
            i, e->name,
            t->position[0], t->position[1], t->position[2],
            t->rotation[0], t->rotation[1], t->rotation[2],
            t->scale[0], t->scale[1], t->scale[2]);

        SDL_WriteIO(file, buffer, SDL_strlen(buffer));

        if (m->model) {
            const char* model_id =
                kowai_asset_bank_get_id_from_model(asset_bank, m->model);

            SDL_snprintf(buffer, sizeof(buffer),
                "has_mesh=1\nmodel_name=%s\n",
                model_id ? model_id : "unknown");
        }
        else {
            SDL_snprintf(buffer, sizeof(buffer),
                "has_mesh=0\nmodel_name=none\n");
        }

        SDL_WriteIO(file, buffer, SDL_strlen(buffer));
    }

    SDL_CloseIO(file);
    return true;
}

// =====================================================
// LOAD SCENE (FIX REAL DEL CRASH)
// =====================================================
KowaiScene* kowai_scene_load(const char* filepath, KowaiAssetBank* asset_bank) {
    if (!filepath) return NULL;

    SDL_IOStream* file = SDL_IOFromFile(filepath, "r");
    if (!file) return NULL;

    KowaiScene* scene = kowai_scene_create("LoadedScene");
    if (!scene) {
        SDL_CloseIO(file);
        return NULL;
    }

    char line[256];
    int current_entity_idx = -1;
    KowaiEntity* current_entity = NULL;

    size_t bytes;

    bool in_entity = false;

    // =====================================================
    // FIX: LECTURA REAL SEGURA
    // =====================================================
    while (kowai_read_line(file, line, sizeof(line))) {
        line[strcspn(line, "\r\n")] = 0;

        if (strncmp(line, "[Scene]", 7) == 0) {
            continue;
        }

        if (strncmp(line, "name=", 5) == 0 && current_entity_idx == -1) {
            SDL_strlcpy(scene->name, line + 5, sizeof(scene->name));
            continue;
        }

        if (strncmp(line, "sun_direction=", 14) == 0) {
            sscanf(line + 14, "%f,%f,%f",
                &scene->sun_direction[0],
                &scene->sun_direction[1],
                &scene->sun_direction[2]);
            continue;
        }

        if (strncmp(line, "[Entity_", 8) == 0)
        {
            in_entity = true;
            sscanf(line + 8, "%d]", &current_entity_idx);

            if (current_entity_idx >= 0 &&
                current_entity_idx < MAX_ENTITIES_PER_SCENE) {

                current_entity = &scene->entities[current_entity_idx];
                scene->entity_count++;
            }
            continue;
        }

        if (!in_entity) continue;

        if (strncmp(line, "name=", 5) == 0) {
            SDL_strlcpy(current_entity->name, line + 5,
                sizeof(current_entity->name));
        }

        if (strncmp(line, "position=", 9) == 0) {
            TransformComponent* t = &scene->transforms[current_entity_idx];
            sscanf(line + 9, "%f,%f,%f",
                &t->position[0],
                &t->position[1],
                &t->position[2]);
        }

        if (strncmp(line, "rotation=", 9) == 0) {
            TransformComponent* t = &scene->transforms[current_entity_idx];
            sscanf(line + 9, "%f,%f,%f",
                &t->rotation[0],
                &t->rotation[1],
                &t->rotation[2]);
        }

        if (strncmp(line, "scale=", 6) == 0) {
            TransformComponent* t = &scene->transforms[current_entity_idx];
            sscanf(line + 6, "%f,%f,%f",
                &t->scale[0],
                &t->scale[1],
                &t->scale[2]);

            kowai_transform_update_matrix(t);
            kowai_entity_add_component(current_entity,
                COMPONENT_TRANSFORM, t);
        }

        if (strncmp(line, "has_mesh=1", 10) == 0) {
            scene->meshes[current_entity_idx].is_visible = true;
        }

        if (strncmp(line, "model_name=", 11) == 0) {
            const char* model_name = line + 11;

            if (strcmp(model_name, "none") != 0 && asset_bank) {
                KowaiModel* model =
                    kowai_asset_bank_get_model(asset_bank, model_name);

                scene->meshes[current_entity_idx].model = model;

                kowai_entity_add_component(
                    current_entity,
                    COMPONENT_MESH_RENDER,
                    &scene->meshes[current_entity_idx]);
            }
        }
    }

    SDL_CloseIO(file);

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

            kowai_entity_init(&scene->entities[i], i, name);

            glm_vec3_copy((vec3) { 0, 0, 0 }, scene->transforms[i].position);
            glm_vec3_copy((vec3) { 0, 0, 0 }, scene->transforms[i].rotation);
            glm_vec3_copy((vec3) { 1, 1, 1 }, scene->transforms[i].scale);

            kowai_transform_update_matrix(&scene->transforms[i]);

            kowai_entity_add_component(
                &scene->entities[i],
                COMPONENT_TRANSFORM,
                &scene->transforms[i]);

            scene->entity_count++;

            return &scene->entities[i];
        }
    }

    return NULL;
}

// =====================================================
// DESTROY ENTITY
// =====================================================
bool kowai_scene_destroy_entity(KowaiScene* scene, uint32_t id) {
    if (!scene || id >= MAX_ENTITIES_PER_SCENE) return false;

    KowaiEntity* e = &scene->entities[id];

    if (e->name[0] == '\0') return false;

    e->name[0] = '\0';
    e->component_count = 0;

    memset(e->components, 0,
        sizeof(e->components));

    scene->meshes[id].model = NULL;
    scene->meshes[id].is_visible = false;

    scene->entity_count--;

    return true;
}