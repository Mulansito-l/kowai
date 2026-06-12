#include <stdlib.h>
#include <SDL3/SDL.h>
#include <KowaiEngine/cglm/cglm.h>

#include "KowaiEngine/camera.h"
#include "KowaiEngine/renderer.h"
#include "KowaiEngine/engine.h"
#include "KowaiEngine/scene.h"
#include "KowaiEngine/asset_bank.h"
#include "KowaiEngine/time.h"
#include "KowaiEngine/imgui_backend.h"

struct KowaiEngine {
    SDL_Window* window;
    KowaiRenderer* renderer;
    bool is_running;
    bool show_devtools;

    KowaiCamera editor_camera;
    KowaiCamera game_camera;
    KowaiCamera* active_camera;

    KowaiTimer* timer;

    // Sistemas macros genéricos del motor
    KowaiAssetBank asset_bank;
    KowaiScene* active_scene;
    char project_path[256]; // 🟢 Nueva variable
};

KowaiEngine* kowai_init(const char* title, int width, int height) {
    KowaiEngine* engine = malloc(sizeof(KowaiEngine));
    if (!engine) return NULL;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        free(engine);
        return NULL;
    }

    engine->window = SDL_CreateWindow(title, width, height, 0);
    if (!engine->window) {
        SDL_Quit();
        free(engine);
        return NULL;
    }

    // 1. Crear el Renderizador base
    engine->renderer = kowai_renderer_create(engine->window);
    if (!engine->renderer) {
        SDL_DestroyWindow(engine->window);
        SDL_Quit();
        free(engine);
        return NULL;
    }

    // 2. Inicializar subsistemas del motor (Tiempo y Asset Bank)
    engine->timer = kowai_timer_create(60.0);
    kowai_asset_bank_init(&engine->asset_bank);

    engine->active_scene = NULL; // Se inyectará después desde KowaiTest
    engine->is_running = true;
    engine->show_devtools = false;

    // 3. Inicializar cámaras
    kowai_camera_init(&engine->editor_camera, CAMERA_MODE_EDITOR);
    kowai_camera_init(&engine->game_camera, CAMERA_MODE_GAME);
    engine->active_camera = &engine->editor_camera;

    // 4. Inicializar backend de interfaz gráfica
    SDL_GPUDevice* device = kowai_renderer_get_device(engine->renderer);
    SDL_GPUTextureFormat swapchain_format = SDL_GetGPUSwapchainTextureFormat(device, engine->window);
    imgui_backend_init(engine->window, device, swapchain_format);

    return engine;
}

void kowai_update(KowaiEngine* engine) {
    if (!engine) return;

    // 1. Capturar estados del mouse para el vuelo de la cámara
    float mouse_dx = 0.0f, mouse_dy = 0.0f;
    Uint32 mouse_buttons = SDL_GetRelativeMouseState(&mouse_dx, &mouse_dy);

    // 2. Procesar Eventos del Sistema
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        imgui_backend_process_event(&event);

        if (event.type == SDL_EVENT_QUIT) {
            engine->is_running = false;
        }
        else if (event.type == SDL_EVENT_KEY_DOWN) {
            if (event.key.key == SDLK_F2) {
                engine->show_devtools = !engine->show_devtools;
                engine->active_camera = engine->show_devtools ? &engine->editor_camera : &engine->game_camera;
                SDL_Log("KowaiEngine: Cambiando a Camara de %s", engine->show_devtools ? "Editor" : "Juego");
            }
        }
    }

    // 3. Actualizar el reloj global del motor
    kowai_timer_update(engine->timer);

    // 4. BUCLE FIJO (Simulación y actualización espacial a 60Hz)
    while (engine->timer->accumulator >= engine->timer->fixed_delta_time) {

        // A) Procesar Input de la cámara de desarrollo
        if (engine->active_camera->mode == CAMERA_MODE_EDITOR) {
            const Uint8* keyboard_state = SDL_GetKeyboardState(NULL);

            if ((mouse_buttons & SDL_BUTTON_RMASK)) {
                SDL_HideCursor();
                kowai_camera_process_input(engine->active_camera, keyboard_state, mouse_dx, mouse_dy, (float)engine->timer->fixed_delta_time);
            }
            else {
                SDL_ShowCursor();
            }
        }

        // B) Actualizar las matrices TRS de las entidades que tengan un Transform
        if (engine->active_scene) {
            for (int i = 0; i < MAX_ENTITIES_PER_SCENE; i++) {
                KowaiEntity* entity = &engine->active_scene->entities[i];

                // Si el nombre está vacío, es un slot inactivo, lo saltamos
                if (!entity->name || entity->name[0] == '\0') continue;

                // Usamos tu función nativa para buscar el componente
                TransformComponent* transform = (TransformComponent*)kowai_entity_get_component(entity, COMPONENT_TRANSFORM);
                if (transform) {
                    kowai_transform_update_matrix(transform);
                }
            }
        }

        engine->timer->accumulator -= engine->timer->fixed_delta_time;
    }

    // 5. Matrices de proyección de la cámara activa
    int w, h;
    SDL_GetWindowSizeInPixels(engine->window, &w, &h);
    kowai_camera_update_matrices(engine->active_camera, w, h);

    // 6. PIPELINE DE RENDERIZADO
    if (kowai_renderer_begin_frame(engine->renderer, engine->window))
    {
        // PASS 1: Dibujar mundo 3D (Solo si hay una escena activa cargada)
        kowai_renderer_begin_render_pass_3d(engine->renderer);

        if (engine->active_scene) {
            KowaiScene* scene = engine->active_scene;

            for (int i = 0; i < MAX_ENTITIES_PER_SCENE; i++) {
                KowaiEntity* entity = &scene->entities[i];

                // Si el slot está vacío, lo saltamos
                if (!entity->name || entity->name[0] == '\0') continue;

                // 🟢 SOLUCIÓN: En lugar de preguntar al sistema ECS dinámico,
                // accedemos directamente a los arreglos de datos usando la ID real de la entidad
                uint32_t id = entity->id;

                // Extraemos las referencias directas desde los pools de la escena
                TransformComponent* transform = &scene->transforms[id]; // Ajusta el nombre si se llama diferente en tu Scene struct (ej: scene->transforms)
                MeshRenderComponent* mesh_render = &scene->meshes[id];

                // Validamos si la malla tiene un modelo cargado y está marcada como visible
                if (mesh_render->model && mesh_render->is_visible) {
                    kowai_renderer_draw_model(
                        engine->renderer,
                        mesh_render->model,
                        transform->matrix, // Mandamos su matriz TRS actualizada
                        engine->active_camera
                    );
                }
            }
        }

        kowai_renderer_end_render_pass(engine->renderer);

        // PASS 2: Dibujar herramientas de desarrollo (UI)
        if (engine->show_devtools) {
            // Pasamos los datos dinámicos de la escena activa a ImGui (si no hay, pasamos NULL)
            imgui_backend_build_devtools(
                engine,
                (float)engine->timer->delta_time);
            imgui_backend_prepare(kowai_renderer_get_current_cmd_buffer(engine->renderer));

            kowai_renderer_begin_render_pass_ui(engine->renderer);

            imgui_backend_render(
                kowai_renderer_get_current_cmd_buffer(engine->renderer),
                kowai_renderer_get_current_render_pass(engine->renderer)
            );

            kowai_renderer_end_render_pass(engine->renderer);
        }
        else {
            imgui_backend_skip_frame();
        }

        kowai_renderer_end_frame(engine->renderer);
    }

    // 7. Liberar CPU
    SDL_DelayNS(1000000);
}

void kowai_shutdown(KowaiEngine* engine) {
    if (!engine) return;

    SDL_GPUDevice* device = kowai_renderer_get_device(engine->renderer);

    // Limpieza automática y segura de toda la VRAM registrada en el banco
    kowai_asset_bank_clear(device, &engine->asset_bank);

    kowai_renderer_destroy(engine->renderer);
    SDL_ReleaseWindowFromGPUDevice(NULL, engine->window);
    SDL_DestroyWindow(engine->window);
    SDL_Quit();

    kowai_timer_destroy(engine->timer);
    free(engine);
}

// 🟢 GETTERS Y SETTERS PÚBLICOS PARA LA API (Lo que usará KowaiTest)

KowaiAssetBank* kowai_engine_get_asset_bank(KowaiEngine* engine) {
    return engine ? &engine->asset_bank : NULL;
}

KowaiScene* kowai_engine_get_active_scene(KowaiEngine* engine)
{
    return engine ? engine->active_scene : NULL;
}

KowaiRenderer* kowai_get_renderer(KowaiEngine* engine) {
    return engine ? engine->renderer : NULL;
}

void kowai_engine_set_active_scene(KowaiEngine* engine, KowaiScene* scene) {
    if (engine) {
        engine->active_scene = scene;
        SDL_Log("KowaiEngine: Escena '%s' enlazada al motor con exito.", scene ? scene->name : "NULL");
    }
}

void kowai_engine_set_project_path(KowaiEngine* engine, const char* path) {
    if (engine && path) {
        SDL_strlcpy(engine->project_path, path, sizeof(engine->project_path));
        SDL_Log("KowaiEngine: Ruta de proyecto establecida: %s", engine->project_path);
    }
}

const char* kowai_engine_get_project_path(KowaiEngine* engine) {
    return engine ? engine->project_path : NULL;
}

bool kowai_is_running(KowaiEngine* engine) {
    return engine ? engine->is_running : false;
}