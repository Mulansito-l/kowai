#ifndef KOWAI_MODEL_H
#define KOWAI_MODEL_H

#include "KowaiEngine/mesh.h"

// El contenedor de alto nivel que expone tu motor
typedef struct KowaiModel{
    KowaiMesh* meshes;    // Array dinámico de sub-mallas
    Uint32 mesh_count;    // Total de mallas en este modelo
} KowaiModel;

KowaiModel* kowai_model_load_gltf(SDL_GPUDevice* gpu_device, const char* filepath);
SDL_GPUTexture* kowai_internal_load_texture(SDL_GPUDevice* gpu_device, const char* filepath);
void kowai_model_destroy(SDL_GPUDevice* gpu_device, KowaiModel* model);

#endif // KOWAI_MODEL_H