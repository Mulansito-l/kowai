#ifndef KOWAI_SCENE_H
#define KOWAI_SCENE_H

#include "KowaiEngine/entity.h"
#include <KowaiEngine/cglm/cglm.h>
#include "asset_bank.h"

#define MAX_ENTITIES_PER_SCENE 128

typedef struct KowaiScene {
    char name[64];
    char current_filepath[512];

    // El almacenamiento ECS que antes estaba en engine.c, ahora encapsulado aquí
    int entity_count;
    KowaiEntity entities[MAX_ENTITIES_PER_SCENE];
    TransformComponent transforms[MAX_ENTITIES_PER_SCENE];
    MeshRenderComponent meshes[MAX_ENTITIES_PER_SCENE];
    LightComponent lights[MAX_ENTITIES_PER_SCENE];

    // Propiedades de la atmósfera de esta escena en específico
    vec3 sun_direction;
    vec3 sun_color;        // R, G, B
    vec3 ambient_color;    // R, G, B
    vec3 sky_clear_color;  // R, G, B (Color del fondo de ventana)
} KowaiScene;

// Ciclo de vida de la escena
KowaiScene* kowai_scene_create(const char* name);
void kowai_scene_destroy(KowaiScene* scene);
void kowai_scene_clear(KowaiScene* scene);

bool kowai_scene_save(KowaiScene* scene, KowaiAssetBank* asset_bank, const char* filepath);
KowaiScene* kowai_scene_load(const char* filepath, KowaiAssetBank* asset_bank, SDL_GPUDevice* gpu_device);

// Gestión de entidades dentro de la escena
KowaiEntity* kowai_scene_create_entity(KowaiScene* scene, const char* name);
bool kowai_scene_destroy_entity(KowaiScene* scene, uint32_t entity_id);

void ensure_kscene_extension(const char* input, char* output, size_t size);

#endif // KOWAI_SCENE_H