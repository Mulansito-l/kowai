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
#include "KowaiEngine/input.h"

struct KowaiEngine {
    SDL_Window* window;
    KowaiRenderer* renderer;
    bool is_running;
    bool show_devtools;

    KowaiCamera editor_camera;
    KowaiCamera game_camera;
    KowaiCamera* active_camera;

    KowaiTimer* timer;

    KowaiInputSystem input_system;

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

    kowai_input_init(&engine->input_system);
    kowai_engine_create_editor_input_context(engine);

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

    // 🟢 1. ACTUALIZAR EL SISTEMA DE INPUT GLOBAL
    // Guardamos los estados del frame anterior antes de procesar los nuevos
    kowai_input_update(&engine->input_system);

    // Recuperamos los deltas del mouse de forma nativa a través de SDL3
    float mouse_dx = 0.0f, mouse_dy = 0.0f;
    SDL_GetRelativeMouseState(&mouse_dx, &mouse_dy);

    // 2. Procesar Eventos del Sistema
    SDL_Event event;
    while (SDL_PollEvent(&event)) {

        // 🟢 Si ImGui captura el evento, hacemos continue inmediatamente.
        // Esto evita que se disparen atajos o lógicas del motor por detrás de la UI.
        if (imgui_backend_process_event(&event)) {
            continue;
        }

        if (event.type == SDL_EVENT_QUIT) {
            engine->is_running = false;
        }
        else if (event.type == SDL_EVENT_KEY_DOWN) {
            if (event.key.key == SDLK_F2) {
                engine->show_devtools = !engine->show_devtools;
                engine->active_camera = engine->show_devtools ? &engine->editor_camera : &engine->game_camera;

                kowai_input_set_context_active(&engine->input_system, "KowaiEditor", engine->show_devtools);
                kowai_input_set_context_active(&engine->input_system, "Gameplay", !engine->show_devtools);

                SDL_Log("KowaiEngine: Cambiando a Camara de %s", engine->show_devtools ? "Editor" : "Juego");
            }
        }
    }

    // Atajos rápidos globales del editor usando tu nuevo Input System
    if (engine->show_devtools && kowai_input_get_action_down(&engine->input_system, "SaveScene")) {
        if (engine->active_scene && engine->active_scene->current_filepath[0] != '\0') {
            kowai_scene_save(engine->active_scene, &engine->asset_bank, engine->active_scene->current_filepath);
            SDL_Log("KowaiEngine: Escena guardada mediante atajo rápido (Ctrl+S).");
        }
    }

    // 3. Actualizar el reloj global del motor
    kowai_timer_update(engine->timer);

    // 4. BUCLE FIJO (Simulación y actualización espacial a 60Hz)
    while (engine->timer->accumulator >= engine->timer->fixed_delta_time) {

        // 🟢 A) PROCESAR INPUT DE LA CÁMARA USANDO TU INPUT MAP
        if (engine->active_camera->mode == CAMERA_MODE_EDITOR) {

            // Consultamos la acción abstracta del mouse que definiste en tu setup ("LookAround" o similar)
            // que internamente mapea a INPUT_TYPE_MOUSE_BUTTON -> SDL_BUTTON_RIGHT
            if (kowai_input_get_action(&engine->input_system, "CameraFlightMode")) {
                SDL_HideCursor();

                // Pasamos directamente el puntero del InputSystem a la cámara para que consulte
                // de forma interna "MoveForward", "MoveBackward", "MoveLeft", "MoveRight"
                kowai_camera_process_input_mapped(
                    engine->active_camera,
                    &engine->input_system,
                    mouse_dx,
                    mouse_dy,
                    (float)engine->timer->fixed_delta_time
                );
            }
            else {
                SDL_ShowCursor();
            }
        }

        // B) Actualizar las matrices TRS de las entidades (Tu código actual idéntico...)
        if (engine->active_scene) {
            for (int i = 0; i < MAX_ENTITIES_PER_SCENE; i++) {

                KowaiEntity* entity = &engine->active_scene->entities[i];
                if (!entity->name || entity->name[0] == '\0') continue;

                TransformComponent* transform = (TransformComponent*)kowai_entity_get_component(entity, COMPONENT_TRANSFORM);
                if (transform) {
                    kowai_transform_update_matrix(transform);
                }


            }
        }

        engine->timer->accumulator -= engine->timer->fixed_delta_time;
    }

    // 5. Matrices de proyección de la cámara activa (Tu código actual...)
    int w, h;
    SDL_GetWindowSizeInPixels(engine->window, &w, &h);
    kowai_camera_update_matrices(engine->active_camera, w, h);

    if (engine->active_scene != NULL) {
        KowaiScene* scene = engine->active_scene;
        for (int i = 0; i < MAX_ENTITIES_PER_SCENE; i++) {
            if (scene->entities[i].name[0] != '\0' && scene->entities[i].parent == NULL) {
                // Pequeña corrección de tipos sobre tu código base original para evitar warnings:
                // kowai_transform_update_matrix requiere un TransformComponent*, no una Entity*
                TransformComponent* root_t = (TransformComponent*)kowai_entity_get_component(&scene->entities[i], COMPONENT_TRANSFORM);
                if (root_t) kowai_transform_update_matrix(root_t);
            }
        }
    }

    // 6. PIPELINE DE RENDERIZADO (Tu código actual se queda exactamente igual...)
    if (kowai_renderer_begin_frame(engine->renderer, engine->window))
    {
        if (engine->active_scene) {
            KowaiScene* scene = engine->active_scene;
            kowai_renderer_set_environment(engine->renderer, scene->sky_clear_color, scene->sun_direction, scene->sun_color, scene->ambient_color);
        }
        else {
            kowai_renderer_set_environment(engine->renderer, NULL, NULL, NULL, NULL);
        }

        kowai_renderer_begin_render_pass_3d(engine->renderer);
        if (engine->active_scene) {
            KowaiScene* scene = engine->active_scene;
            for (int i = 0; i < MAX_ENTITIES_PER_SCENE; i++) {
                KowaiEntity* entity = &scene->entities[i];
                if (!entity->name || entity->name[0] == '\0') continue;
                uint32_t id = entity->id;
                TransformComponent* transform = &scene->transforms[id];
                MeshRenderComponent* mesh_render = &scene->meshes[id];

                if (mesh_render->model && mesh_render->is_visible) {
                    kowai_renderer_draw_model(engine->renderer, mesh_render->model, transform->global_matrix, engine->active_camera, engine->active_scene);
                }
            }
        }
        kowai_renderer_end_render_pass(engine->renderer);

        if (engine->show_devtools) {
            imgui_backend_build_devtools(engine, (float)engine->timer->delta_time);
            imgui_backend_prepare(kowai_renderer_get_current_cmd_buffer(engine->renderer));
            kowai_renderer_begin_render_pass_ui(engine->renderer);
            imgui_backend_render(kowai_renderer_get_current_cmd_buffer(engine->renderer), kowai_renderer_get_current_render_pass(engine->renderer));
            kowai_renderer_end_render_pass(engine->renderer);
        }
        else {
            imgui_backend_skip_frame();
        }
        kowai_renderer_end_frame(engine->renderer);
    }

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

KowaiCamera* kowai_engine_get_active_camera(KowaiEngine* engine)
{
    return engine ? engine->active_camera : NULL;
}

KowaiInputSystem kowai_engine_get_input_system(KowaiEngine* engine)
{
    return engine->input_system;
}

void kowai_engine_create_editor_input_context(KowaiEngine* engine)
{
    // =========================================================================
    // 🟢 CONFIGURACIÓN INICIAL DEL MAPA DE ACCIONES (INPUT MAP)
    // =========================================================================

    // 1. Inicializar la estructura limpia en memoria
    kowai_input_init(&engine->input_system);

    // 2. Crear el contexto exclusivo para las herramientas del Editor
    kowai_input_create_context(&engine->input_system, "KowaiEditor");
    kowai_input_set_context_read_only(
        &engine->input_system,
        "KowaiEditor",
        true
    );

    // 3. Mapear el Clic Derecho del mouse a la acción que activa el vuelo de la cámara
    kowai_input_add_mapping(&engine->input_system, "KowaiEditor", "CameraFlightMode",
        INPUT_TYPE_MOUSE_BUTTON, SDL_BUTTON_RIGHT, 0);

    // 4. Mapear los controles de traslación de la cámara para el vuelo 3D
    kowai_input_add_mapping(&engine->input_system, "KowaiEditor", "MoveForward",
        INPUT_TYPE_KEYBOARD, SDL_SCANCODE_W, 0);
    kowai_input_add_mapping(&engine->input_system, "KowaiEditor", "MoveBackward",
        INPUT_TYPE_KEYBOARD, SDL_SCANCODE_S, 0);
    kowai_input_add_mapping(&engine->input_system, "KowaiEditor", "MoveLeft",
        INPUT_TYPE_KEYBOARD, SDL_SCANCODE_A, 0);
    kowai_input_add_mapping(&engine->input_system, "KowaiEditor", "MoveRight",
        INPUT_TYPE_KEYBOARD, SDL_SCANCODE_D, 0);

    // 5. Mapear las Hotkeys globales del Editor que procesa el motor
    kowai_input_add_mapping(&engine->input_system, "KowaiEditor", "SaveScene",
        INPUT_TYPE_KEYBOARD, SDL_SCANCODE_S, SDL_KMOD_CTRL);

    // 6. ¡CRÍTICO! Activar el contexto por defecto para que kowai_input_get_action lo evalúe
    kowai_input_set_context_active(&engine->input_system, "KowaiEditor", true);
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

KowaiInputSystem* kowai_engine_get_input_system_ptr(KowaiEngine* engine)
{
    return engine ? &engine->input_system : NULL;
}

bool kowai_is_running(KowaiEngine* engine) {
    return engine ? engine->is_running : false;
}