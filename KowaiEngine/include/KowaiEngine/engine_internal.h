#ifndef KOWAI_ENGINE_INTERNAL_H
#define KOWAI_ENGINE_INTERNAL_H

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
    char project_path[256]; //Nueva variable

    KowaiStartFn    on_start;
    KowaiUpdateFn   on_update;
    KowaiShutdownFn on_shutdown;

    KowaiPlayMode play_mode;
};

KowaiEngine* kowai_get_current_engine(void);

#endif