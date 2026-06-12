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

    // 2. Registrar los assets que el editor usara
    SDL_GPUDevice* device = kowai_renderer_get_device(kowai_get_renderer(engine));
    KowaiAssetBank* bank = kowai_engine_get_asset_bank(engine);

    kowai_asset_bank_register_gltf(device, bank, "cat_statue", "D:\\Proyectos\\Kowai\\KowaiTest\\models\\concrete_cat_statue_1k\\concrete_cat_statue_1k.gltf");

    // 3. Opcional: Cargar una escena inicial
    // KowaiScene* inicio = kowai_scene_create("EscenaInicial");
    // kowai_engine_set_active_scene(engine, inicio);

    while (kowai_is_running(engine)) {
        kowai_update(engine);
    }

    kowai_shutdown(engine);
    return 0;
}