#ifndef KOWAI_ENGINE_H
#define KOWAI_ENGINE_H

#include <stdbool.h>

// Estructura que encapsula el estado interno del motor (Ventana, GPU, etc.)
typedef struct KowaiEngine KowaiEngine;
typedef struct KowaiScene KowaiScene;
typedef struct KowaiAssetBank KowaiAssetBank;
typedef struct KowaiCamera KowaiCamera;
typedef struct KowaiInputSystem KowaiInputSystem;

typedef enum {
    KOWAI_MODE_EDITOR,
    KOWAI_MODE_PLAYING,
    KOWAI_MODE_PAUSED
} KowaiPlayMode;

// Funciones de ciclo de vida del motor
KowaiEngine* kowai_init(const char* title, int width, int height, const char* project_path);
bool kowai_is_running(KowaiEngine* engine);
void kowai_update(KowaiEngine* engine);
void kowai_shutdown(KowaiEngine* engine);
// Añade esto a las funciones públicas
void kowai_engine_set_project_path(KowaiEngine* engine, const char* path);
const char* kowai_engine_get_project_path(KowaiEngine* engine);
KowaiAssetBank* kowai_engine_get_asset_bank(KowaiEngine* engine);
KowaiScene* kowai_engine_get_active_scene(KowaiEngine* engine);
void kowai_engine_set_active_scene(KowaiEngine* engine, KowaiScene* scene);
KowaiCamera* kowai_engine_get_active_camera(KowaiEngine* engine);
KowaiInputSystem kowai_engine_get_input_system(KowaiEngine* engine);
KowaiInputSystem* kowai_engine_get_input_system_ptr(KowaiEngine* engine);

KowaiPlayMode kowai_engine_get_play_mode();

void kowai_engine_play(KowaiEngine* engine);
void kowai_engine_pause(KowaiEngine* engine);
void kowai_engine_stop(KowaiEngine* engine);

void kowai_engine_create_editor_input_context(KowaiEngine* engine);

KowaiEngine* kowai_get_current_engine(void);

typedef void (*KowaiStartFn)(void);
typedef void (*KowaiUpdateFn)(float delta_time);
typedef void (*KowaiShutdownFn)(void);

void kowai_engine_set_game_callbacks(
    KowaiEngine* engine,
    KowaiStartFn on_start,
    KowaiUpdateFn on_update,
    KowaiShutdownFn on_shutdown);

#endif // KOWAI_ENGINE_H