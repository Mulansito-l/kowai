#ifndef KOWAI_ENGINE_H
#define KOWAI_ENGINE_H

#include <stdbool.h>

// Estructura que encapsula el estado interno del motor (Ventana, GPU, etc.)
typedef struct KowaiEngine KowaiEngine;
typedef struct KowaiScene KowaiScene;
typedef struct KowaiAssetBank KowaiAssetBank;
typedef struct KowaiCamera KowaiCamera;

// Funciones de ciclo de vida del motor
KowaiEngine* kowai_init(const char* title, int width, int height);
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

#endif // KOWAI_ENGINE_H