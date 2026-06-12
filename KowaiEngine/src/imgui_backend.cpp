#include "KowaiEngine/imgui_backend.h"

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlgpu3.h"
#include <nfd.h>

extern "C" {

#include "KowaiEngine/engine.h"
#include "KowaiEngine/scene.h"
#include "KowaiEngine/asset_bank.h"
#include "KowaiEngine/entity.h"
#include "KowaiEngine/renderer.h"

    void imgui_backend_init(SDL_Window* window, SDL_GPUDevice* device, SDL_GPUTextureFormat format) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // Activa el docking de ventanas

        ImGui_ImplSDL3_InitForSDLGPU(window);

        ImGui_ImplSDLGPU3_InitInfo info = {};
        info.Device = device;
        info.ColorTargetFormat = format;

        ImGui_ImplSDLGPU3_Init(&info);

        SDL_Log("ImGui backend inicializado.");
    }

    void imgui_backend_shutdown(void) {
        ImGui_ImplSDLGPU3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        SDL_Log("ImGui backend destruido.");
    }

    void imgui_backend_new_frame(void) {
        ImGui_ImplSDLGPU3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
    }

    void imgui_backend_build_devtools(KowaiEngine* engine, float delta_time) {
        if (!engine) return;

        // --- VENTANA 1: ESTADÍSTICAS ---
        ImGui::Begin("Estadísticas del Motor");
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::Text("Delta Time: %.4f s (%.2f ms)", delta_time, delta_time * 1000.0f);

        static float frame_history[120] = { 0 };
        static int offset = 0;
        frame_history[offset] = delta_time * 1000.0f;
        offset = (offset + 1) % 120;

        ImGui::PlotLines("Tiempos de Frame", frame_history, 120, offset, "ms", 0.0f, 33.0f, ImVec2(0, 50));
        ImGui::End();

        // Getters de la API pública de engine.h
        KowaiAssetBank* asset_bank = kowai_engine_get_asset_bank(engine);
        const char* project_path = kowai_engine_get_project_path(engine);
        KowaiScene* scene = kowai_engine_get_active_scene(engine);

        // --- VENTANA 2: PROJECT & SCENE MANAGER ---
        ImGui::Begin("Kowai Engine - Project & Scene Manager");

        ImGui::Text("Proyecto: %s", project_path ? project_path : "No definido");
        ImGui::Separator();

        static char nombre_escena[32] = "NuevaEscena";
        ImGui::InputText("Nombre de Escena", nombre_escena, 32);

        if (ImGui::Button("Crear Nueva Escena (Archivo .kscene)")) {
            KowaiScene* nueva = kowai_scene_create(nombre_escena);
            if (nueva) {
                if (scene) {
                    kowai_scene_destroy(scene);
                }
                kowai_engine_set_active_scene(engine, nueva);
                scene = nueva;
            }
        }

        ImGui::Separator();

        ImGui::Text("Persistencia de Escena");

        if (ImGui::Button("Guardar Escena Actual"))
        {
            nfdchar_t* outPath = NULL;

            nfdresult_t result = NFD_SaveDialog("kscene", project_path, &outPath);

            if (result == NFD_OKAY)
            {
                if (scene)
                {
                    char finalPath[512];

                    ensure_kscene_extension(outPath, finalPath, sizeof(finalPath));

                    kowai_scene_save(scene, asset_bank, finalPath);
                }

                free(outPath);
            }
            else if (result == NFD_CANCEL)
            {
                SDL_Log("Save cancelado");
            }
            else
            {
                SDL_Log("Error en NFD Save Dialog");
            }
        }

        ImGui::SameLine();

        if (ImGui::Button("Cargar Escena"))
        {
            nfdchar_t* outPath = NULL;

            nfdresult_t result = NFD_OpenDialog("kscene", project_path, &outPath);

            if (result == NFD_OKAY)
            {
                KowaiScene* cargada = kowai_scene_load(outPath, asset_bank);

                if (cargada)
                {
                    if (scene)
                    {
                        kowai_scene_destroy(scene);
                    }

                    kowai_engine_set_active_scene(engine, cargada);
                    scene = cargada;
                }

                free(outPath);
            }
            else if (result == NFD_CANCEL)
            {
                SDL_Log("Load cancelado");
            }
            else
            {
                SDL_Log("Error en NFD Open Dialog");
            }
        }

        ImGui::Separator();

        // --- SECCIÓN: ENTIDADES ---
        if (scene != NULL) {
            ImGui::Text("Escena: %s | Entidades: %d", scene->name, scene->entity_count);

            if (ImGui::Button("Spawnear Entidad Vacia")) {
                kowai_scene_create_entity(scene, "Nueva Entidad");
            }

            ImGui::SameLine();

            if (ImGui::Button("Spawnear Estatua de Gato")) {
                // 1. Creamos el formato de nombre en un buffer local temporal
                char buffer_nombre[32];
                SDL_snprintf(buffer_nombre, sizeof(buffer_nombre), "Michi_%d", scene->entity_count);

                // 🟢 CORREGIDO: Pasamos el buffer local directo. 
                // kowai_scene_create_entity internamente hará el SDL_strlcpy a su propia memoria fija.
                KowaiEntity* e = kowai_scene_create_entity(scene, buffer_nombre);
                if (e) {
                    KowaiModel* michi_model = kowai_asset_bank_get_model(asset_bank, "cat_statue");
                    uint32_t id = e->id;

                    scene->meshes[id].model = michi_model;
                    scene->meshes[id].is_visible = true;

                    kowai_entity_add_component(e, COMPONENT_MESH_RENDER, &scene->meshes[id]);
                }
            }
        }
        else {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "No hay ninguna escena activa.");
        }

        ImGui::End();

        // --- VENTANA 3: INSPECTOR DE JERARQUÍA Y COMPONENTES ---
        static int seleccionado_idx = -1;

        ImGui::Begin("Kowai Engine - Inspector");
        if (scene != NULL) {
            // Columna Izquierda: Jerarquía
            ImGui::BeginChild("Hierarchy", ImVec2(200, 0), true);
            for (int i = 0; i < MAX_ENTITIES_PER_SCENE; i++) {
                // 🟢 CORREGIDO: Validamos que el slot no esté vacío revisando el primer caracter
                if (scene->entities[i].name[0] != '\0') {
                    char label_imgui[64];
                    SDL_snprintf(label_imgui, sizeof(label_imgui), "%s##ent_%d", scene->entities[i].name, i);

                    if (ImGui::Selectable(label_imgui, seleccionado_idx == i)) {
                        seleccionado_idx = i;
                    }
                }
            }
            ImGui::EndChild();

            ImGui::SameLine();

            // Columna Derecha: Componentes
            ImGui::BeginChild("Components");
            // 🟢 CORREGIDO: El chequeo de existencia ahora es con el primer caracter del arreglo, no con NULL
            if (seleccionado_idx != -1 && scene->entities[seleccionado_idx].name[0] != '\0') {
                KowaiEntity* e = &scene->entities[seleccionado_idx];

                // --- Transform Component ---
                TransformComponent* t = (TransformComponent*)kowai_entity_get_component(e, COMPONENT_TRANSFORM);
                if (t) {
                    ImGui::Text("--- TRANSFORM ---");
                    if (ImGui::DragFloat3("Pos", t->position, 0.1f)) kowai_transform_update_matrix(t);
                    if (ImGui::DragFloat3("Rot", t->rotation, 1.0f)) kowai_transform_update_matrix(t);
                    if (ImGui::DragFloat3("Scale", t->scale, 0.1f))  kowai_transform_update_matrix(t);
                }

                // --- Mesh Component ---
                MeshRenderComponent* m = (MeshRenderComponent*)kowai_entity_get_component(e, COMPONENT_MESH_RENDER);
                if (m) {
                    ImGui::Separator();
                    ImGui::Text("--- MESH RENDER ---");
                    ImGui::Checkbox("Visible", &m->is_visible);
                    ImGui::Text("Model: %s", m->model ? "Asignado" : "NULL");
                }
            }
            ImGui::EndChild();
        }
        else {
            ImGui::Text("Crea una escena para inspeccionar sus entidades.");
        }
        ImGui::End();

        // Finaliza el frame de ImGui
        ImGui::Render();
    }

    void imgui_backend_prepare(SDL_GPUCommandBuffer* cmd) {
        ImGui_ImplSDLGPU3_PrepareDrawData(ImGui::GetDrawData(), cmd);
    }

    void imgui_backend_render(SDL_GPUCommandBuffer* cmd, SDL_GPURenderPass* pass) {
        ImGui_ImplSDLGPU3_RenderDrawData(ImGui::GetDrawData(), cmd, pass, nullptr);
    }

    bool imgui_backend_process_event(SDL_Event* event) {
        return ImGui_ImplSDL3_ProcessEvent(event);
    }

    void imgui_backend_skip_frame(void) {
        ImGui::EndFrame();
    }

} // extern "C"