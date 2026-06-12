#ifndef KOWAI_MESH_H
#define KOWAI_MESH_H

#include <SDL3/SDL.h>

// La unidad geométrica atómica que se envía a la GPU en un solo Draw Call
typedef struct {
    SDL_GPUBuffer* vertex_buffer;
    SDL_GPUBuffer* index_buffer;
    Uint32 index_count;

    SDL_GPUTexture* texture;
    SDL_GPUSampler* sampler;
} KowaiMesh;

#endif // KOWAI_MESH_H