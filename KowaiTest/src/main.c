#include <SDL3/SDL.h>
#include <KowaiEngine/cglm/cglm.h>

// Cabeceras de la biblioteca del motor
#include <KowaiEngine/engine.h>
#include <KowaiEngine/renderer.h>
#include <KowaiEngine/scene.h>
#include <KowaiEngine/asset_bank.h>
#include <KowaiEngine/entity.h>

int main(int argc, char* argv[]) {
    KowaiEngine* engine = kowai_init("KowaiTest Editor", 1920, 1080);
    if (!engine) {
        SDL_Log("KowaiTest: Error crítico al inicializar el motor.");
        return -1;
    }

    // 1. Establecer la ruta del proyecto (Donde se crearan las escenas)
    // Asegúrate de que la ruta termine en barra diagonal '\'
    kowai_engine_set_project_path(engine, "D:\\Proyectos\\Kowai\\KowaiTest\\");

    while (kowai_is_running(engine)) {
        kowai_update(engine);
    }

    kowai_shutdown(engine);
    return 0;
}