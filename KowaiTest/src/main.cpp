#include <SDL3/SDL.h>
#include <KowaiEngine/cglm/cglm.h>

// Cabeceras de la biblioteca del motor
#include <KowaiEngine/kowai.h>

void on_start(void) {
    SDL_Log("Game Start!");
    Kowai::Input::set_context("Player", true);
}

void on_update(float delta_time) {
    if (Kowai::Input::action_down("Jump"))
        SDL_Log("Jump!");
}

void on_shutdown(void) {
    SDL_Log("Game Shutdown!");
}

int main(int argc, char* argv[]) {
    KowaiEngine* engine = kowai_init("KowaiTest Editor", 1920, 1080, "D:\\Proyectos\\Kowai\\KowaiTest\\");
    
    kowai_engine_set_game_callbacks(engine, on_start, on_update, on_shutdown);

    while (kowai_is_running(engine)) {
        kowai_update(engine);
    }

    kowai_shutdown(engine);
    return 0;
}