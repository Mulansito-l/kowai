#ifndef KOWAI_RENDERER_H
#define KOWAI_RENDERER_H

#include "KowaiEngine/cglm/cglm.h"
#include <SDL3/SDL.h>
#include <stdbool.h>
#include <KowaiEngine/helpers.h>

typedef struct KowaiModel KowaiModel;
typedef struct KowaiRenderer KowaiRenderer;
typedef struct KowaiCamera KowaiCamera; // 🔴 OBLIGATORIO: Le dice a C que esto es un tipo estructurado válido

// Ciclo de vida del renderizador
KowaiRenderer* kowai_renderer_create(SDL_Window* window);
void kowai_renderer_destroy(KowaiRenderer* renderer);

// Control de fotogramas (Frame budget)
bool kowai_renderer_begin_frame(KowaiRenderer* renderer, SDL_Window* window);
void kowai_renderer_begin_render_pass_3d(KowaiRenderer* renderer);
void kowai_renderer_begin_render_pass_ui(KowaiRenderer* renderer);
void kowai_renderer_end_render_pass(KowaiRenderer* renderer);

void kowai_renderer_end_frame(KowaiRenderer* renderer);

void kowai_renderer_set_environment(KowaiRenderer* renderer, float* clear, float* sun_dir, float* sun_col, float* ambient);

SDL_GPUDevice* kowai_renderer_get_device(KowaiRenderer* renderer);
SDL_GPUCommandBuffer* kowai_renderer_get_current_cmd_buffer(KowaiRenderer* renderer);
SDL_GPURenderPass* kowai_renderer_get_current_render_pass(KowaiRenderer* renderer);

void kowai_renderer_draw_model(
    KowaiRenderer* renderer,
    KowaiModel* model,
    mat4 model_matrix,
    KowaiCamera* camera,
    struct KowaiScene* scene);

#endif // KOWAI_RENDERER_H