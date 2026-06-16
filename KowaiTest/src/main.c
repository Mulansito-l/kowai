#include <SDL3/SDL.h>
#include <KowaiEngine/cglm/cglm.h>

// Cabeceras de la biblioteca del motor
#include <KowaiEngine/engine.h>
#include <KowaiEngine/renderer.h>
#include <KowaiEngine/scene.h>
#include <KowaiEngine/input.h>
#include <KowaiEngine/asset_bank.h>
#include <KowaiEngine/entity.h>

int main(int argc, char* argv[]) {
    KowaiEngine* engine = kowai_init("KowaiTest Editor", 1920, 1080, "D:\\Proyectos\\Kowai\\KowaiTest\\");
    if (!engine) {
        SDL_Log("KowaiTest: Error crítico al inicializar el motor.");
        return -1;
    }

    while (kowai_is_running(engine)) {
        kowai_update(engine);
        KowaiInputSystem* input = kowai_engine_get_input_system_ptr(engine);
        if (kowai_input_get_action_down(input, "Jump"))
            SDL_Log("Jump!");
    }

    kowai_shutdown(engine);
    return 0;
}