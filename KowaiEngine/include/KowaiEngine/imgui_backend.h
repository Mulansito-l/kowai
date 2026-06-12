#ifndef KOWAI_IMGUI_BACKEND_H
#define KOWAI_IMGUI_BACKEND_H

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <stdbool.h>

typedef struct KowaiEngine KowaiEngine;
typedef struct KowaiEntity KowaiEntity;

#ifdef __cplusplus
extern "C" {
#endif

	void imgui_backend_init(SDL_Window* window, SDL_GPUDevice* device, SDL_GPUTextureFormat format);
	void imgui_backend_shutdown(void);

	// Llamar al inicio de begin_frame, ANTES de abrir el Render Pass
	void imgui_backend_new_frame(void);

	// Llamar después de imgui_backend_new_frame, construye tu UI aquí
	// fps se muestra en el panel DevTools
	void imgui_backend_build_devtools(KowaiEngine* engine, float delta_time);

	// Llamar antes de abrir el Render Pass — sube vértices a VRAM
	void imgui_backend_prepare(SDL_GPUCommandBuffer* cmd);

	// Llamar dentro del Render Pass activo, al final del frame
	void imgui_backend_render(SDL_GPUCommandBuffer* cmd, SDL_GPURenderPass* pass);

	// Pasar eventos SDL para que ImGui los procese (input)
	bool imgui_backend_process_event(SDL_Event* event);

	void imgui_backend_skip_frame(void);

#ifdef __cplusplus
}
#endif

#endif // KOWAI_IMGUI_BACKEND_H