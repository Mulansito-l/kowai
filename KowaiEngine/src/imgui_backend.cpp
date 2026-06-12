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

    struct KowaiRenderer* kowai_get_renderer(KowaiEngine* engine);
    static void imgui_backend_render_file_tree(KowaiAssetBank* asset_bank, const char* current_dir_path);
    void kowai_entity_remove_component(KowaiEntity* entity, KowaiComponentType type);
    void imgui_backend_draw_asset_browser(KowaiEngine* engine, KowaiAssetBank* asset_bank);

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

    // -----------------------------------------------------
// FUNCIÓN AUXILIAR RECURSIVA PARA EL ÁRBOL
// -----------------------------------------------------
    static void imgui_backend_render_file_tree(KowaiAssetBank* asset_bank, const char* current_dir_path) {
        int count = 0;
        // En SDL3, SDL_GlobDirectory devuelve un array que debe ser liberado correctamente
        char** files = SDL_GlobDirectory(current_dir_path, "*", 0, &count);
        if (!files) return;

        for (int i = 0; i < count; i++) {
            const char* name = files[i];
            if (SDL_strcmp(name, ".") == 0 || SDL_strcmp(name, "..") == 0) continue;

            char full_path[512];
            SDL_snprintf(full_path, sizeof(full_path), "%s/%s", current_dir_path, name);

            SDL_PathInfo info;
            if (SDL_GetPathInfo(full_path, &info) && info.type == SDL_PATHTYPE_DIRECTORY) {
                // 📁 CARPETA
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.75f, 0.30f, 1.0f));
                bool node_open = ImGui::TreeNodeEx(name, ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick);
                ImGui::PopStyleColor();

                if (node_open) {
                    imgui_backend_render_file_tree(asset_bank, full_path);
                    ImGui::TreePop();
                }
            }
            else {
                // 📄 ARCHIVO
                const char* ext = SDL_strrchr(name, '.');
                char label[128];

                if (ext && (SDL_strcasecmp(ext, ".obj") == 0 || SDL_strcasecmp(ext, ".gltf") == 0 || SDL_strcasecmp(ext, ".glb") == 0)) {
                    SDL_snprintf(label, sizeof(label), "🗿 %s", name);
                    ImGui::Selectable(label, false);

                    // 🟢 DRAG SOURCE OPTIMIZADO
                    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                        ImGui::SetDragDropPayload("DND_FILE_PATH", full_path, SDL_strlen(full_path) + 1);

                        ImGui::Text("Preparando modelo: %s", name);
                        ImGui::TextDisabled("(Se cargará en VRAM al soltar)");
                        ImGui::EndDragDropSource();
                    }
                }
                else if (ext && (SDL_strcasecmp(ext, ".png") == 0 || SDL_strcasecmp(ext, ".jpg") == 0)) {
                    SDL_snprintf(label, sizeof(label), "🖼️ %s", name);
                    ImGui::Selectable(label, false);

                    // 🟢 DRAG SOURCE: Arrastrar textura
                    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                        char asset_id[64];
                        SDL_strlcpy(asset_id, name, sizeof(asset_id));
                        char* dot = SDL_strrchr(asset_id, '.');
                        if (dot) *dot = '\0';

                        void* texture = kowai_asset_bank_get_texture(asset_bank, asset_id);
                        if (texture) {
                            ImGui::SetDragDropPayload("DND_TEXTURE", &texture, sizeof(void*));
                            ImGui::Text("Arrastrando textura: %s", asset_id);
                        }
                        else {
                            ImGui::TextDisabled("(No cargada: %s)", asset_id);
                        }
                        ImGui::EndDragDropSource();
                    }
                }
                else if (ext && (SDL_strcasecmp(ext, ".kscene") == 0)) {
                    SDL_snprintf(label, sizeof(label), "🎬 %s", name);
                    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", label);
                }
                else {
                    SDL_snprintf(label, sizeof(label), "📝 %s", name);
                    ImGui::TextDisabled("%s", label);
                }
            }
        }

        // 🟢 FIX DE SEGURIDAD EN SDL3: SDL_free limpia el bloque contenedor, 
        // pero los elementos internos asignados por SDL_GlobDirectory requieren atención nativa si usas empaquetados pesados.
        SDL_free(files);
    }

    // -----------------------------------------------------
// DEV TOOLS MAIN FUNCTION
// -----------------------------------------------------
    void imgui_backend_build_devtools(KowaiEngine* engine, float delta_time) {
        if (!engine) return;

        // Getters de la API pública de engine.h
        KowaiAssetBank* asset_bank = kowai_engine_get_asset_bank(engine);
        const char* project_path = kowai_engine_get_project_path(engine);
        KowaiScene* scene = kowai_engine_get_active_scene(engine);

        // =========================================================================
        // 🟢 TOP BAR CONFIGURADA ESTILO UNITY
        // =========================================================================
        if (ImGui::BeginMainMenuBar()) {

            // --- MENÚ: ARCHIVO (FILE) ---
            if (ImGui::BeginMenu("Archivo")) {

                // 🟢 NUEVA ESCENA (Ahora abre Save Dialog de NFD)
                if (ImGui::MenuItem("Nueva Escena...", "Ctrl+N")) {
                    nfdchar_t* outPath = NULL;
                    nfdresult_t result = NFD_SaveDialog("kscene", project_path, &outPath);

                    if (result == NFD_OKAY) {
                        char finalPath[512];
                        ensure_kscene_extension(outPath, finalPath, sizeof(finalPath));

                        // Extraer el nombre del archivo para dárselo a la estructura interna de la escena
                        const char* filename = SDL_strrchr(finalPath, '/');
                        if (!filename) filename = SDL_strrchr(finalPath, '\\');
                        filename = filename ? filename + 1 : finalPath;

                        char nombre_limpio[32];
                        SDL_strlcpy(nombre_limpio, filename, sizeof(nombre_limpio));
                        char* dot = SDL_strrchr(nombre_limpio, '.');
                        if (dot) *dot = '\0';

                        // Crear la escena en memoria
                        KowaiScene* nueva = kowai_scene_create(nombre_limpio);
                        if (nueva) {
                            if (scene) {
                                kowai_scene_destroy(scene);
                            }
                            // La guardamos inmediatamente en el disco para inicializar el archivo vacío
                            kowai_scene_save(nueva, asset_bank, finalPath);

                            kowai_engine_set_active_scene(engine, nueva);
                            scene = nueva;
                            SDL_Log("VFS: Nueva escena creada físicamente en: %s", finalPath);
                        }
                        free(outPath);
                    }
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Cargar Escena...", "Ctrl+O")) {
                    nfdchar_t* outPath = NULL;
                    nfdresult_t result = NFD_OpenDialog("kscene", project_path, &outPath);

                    if (result == NFD_OKAY) {
                        KowaiScene* cargada = kowai_scene_load(outPath, asset_bank);
                        if (cargada) {
                            if (scene) {
                                kowai_scene_destroy(scene);
                            }
                            kowai_engine_set_active_scene(engine, cargada);
                            scene = cargada;
                        }
                        free(outPath);
                    }
                }

                if (ImGui::MenuItem("Guardar Escena Actual", "Ctrl+S", false, scene != NULL)) {
                    nfdchar_t* outPath = NULL;
                    nfdresult_t result = NFD_SaveDialog("kscene", project_path, &outPath);

                    if (result == NFD_OKAY) {
                        if (scene) {
                            char finalPath[512];
                            ensure_kscene_extension(outPath, finalPath, sizeof(finalPath));
                            kowai_scene_save(scene, asset_bank, finalPath);
                        }
                        free(outPath);
                    }
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Salir del Motor", "Alt+F4")) {
                    // Modificar bandera de cierre aquí si es necesario
                }

                ImGui::EndMenu();
            }

            // --- MENÚ: ESCENA (GAME OBJECTS) ---
            if (ImGui::BeginMenu("Escena", scene != NULL)) { // El menú se deshabilita si no hay escena activa

                if (ImGui::MenuItem("Spawnear Entidad Vacía")) {
                    kowai_scene_create_entity(scene, "Nueva Entidad");
                }

                if (ImGui::MenuItem("Spawnear Estatua de Gato (Michi)")) {
                    char buffer_nombre[32];
                    SDL_snprintf(buffer_nombre, sizeof(buffer_nombre), "Michi_%d", scene->entity_count);

                    KowaiEntity* e = kowai_scene_create_entity(scene, buffer_nombre);
                    if (e) {
                        KowaiModel* michi_model = kowai_asset_bank_get_model(asset_bank, "cat_statue");
                        uint32_t id = e->id;
                        scene->meshes[id].model = michi_model;
                        scene->meshes[id].is_visible = true;
                        kowai_entity_add_component(e, COMPONENT_MESH_RENDER, &scene->meshes[id]);
                    }
                }

                ImGui::EndMenu();
            }

            // --- 🟢 INDICADOR DE ESCENA ABIERTA EN LA TOP BAR ---
            ImGui::Separator();
            if (scene != NULL) {
                ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), "🎬 Escena Activa: [%s]", scene->name);
                ImGui::TextDisabled("| Entidades: %d", scene->entity_count);
            }
            else {
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "⚠️ Sin Escena Activa (Archivo -> Nueva Escena)");
            }

            ImGui::EndMainMenuBar();
        }

        // --- VENTANA 1: ESTADÍSTICAS ---
        ImGui::Begin("Estadísticas del Motor");
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::Text("Delta Time: %.4f s (%.2f ms)", delta_time, delta_time * 1000.0f);

        static float frame_history[120] = { 0 };
        static int offset = 0;
        frame_history[offset] = delta_time * 1000.0f;
        offset = (offset + 1) % 120;

        ImGui::PlotLines("##tiempos", frame_history, 120, offset, "ms", 0.0f, 33.0f, ImVec2(-1, 50));
        ImGui::Text("Tiempos de Frame");
        ImGui::End();

        // =========================================================================
        // 🟢 VENTANA 2 RESTAURADA: EXPLORADOR DE RECURSOS (VFS)
        // =========================================================================
        imgui_backend_draw_asset_browser(engine, asset_bank);

        // --- VENTANA 3: INSPECTOR DE JERARQUÍA Y COMPONENTES ---
        static int seleccionado_idx = -1;
        ImGui::Begin("Kowai Engine - Inspector");

        if (scene != NULL) {
            // -----------------------------------------------------------------
            // COLUMNA IZQUIERDA: JERARQUÍA (Con soporte para eliminar entidad)
            // -----------------------------------------------------------------
            ImGui::BeginChild("Hierarchy", ImVec2(200, 0), true);
            for (int i = 0; i < MAX_ENTITIES_PER_SCENE; i++) {
                if (scene->entities[i].name[0] != '\0') {
                    char label_imgui[64];
                    SDL_snprintf(label_imgui, sizeof(label_imgui), "%s##ent_%d", scene->entities[i].name, i);

                    // Guardamos la selección
                    if (ImGui::Selectable(label_imgui, seleccionado_idx == i)) {
                        seleccionado_idx = i;
                    }

                    if (ImGui::BeginPopupContextItem()) {
                        if (ImGui::MenuItem("❌ Eliminar Entidad")) {
                            SDL_Log("ECS: Destruyendo entidad '%s' (ID: %d)", scene->entities[i].name, i);

                            // FIX: Pasamos el id numérico puro tal como dicta tu firma de C
                            kowai_scene_destroy_entity(scene, scene->entities[i].id);

                            if (seleccionado_idx == i) {
                                seleccionado_idx = -1;
                            }
                        }
                        ImGui::EndPopup();
                    }
                }
            }
            ImGui::EndChild();

            ImGui::SameLine();

            // -----------------------------------------------------------------
            // COLUMNA DERECHA: COMPONENTES INDIVIDUALES
            // -----------------------------------------------------------------
            ImGui::BeginChild("Components");
            if (seleccionado_idx != -1 && scene->entities[seleccionado_idx].name[0] != '\0') {
                KowaiEntity* e = &scene->entities[seleccionado_idx];

                // 📝 Modificar Nombre de la Entidad directamente en la parte superior
                ImGui::Text("GameObject:"); ImGui::SameLine();
                ImGui::InputText("##EntityName", e->name, sizeof(e->name));
                ImGui::Separator();

                // --- 1. Transform Component ---
                TransformComponent* t = (TransformComponent*)kowai_entity_get_component(e, COMPONENT_TRANSFORM);
                if (t) {
                    bool open = ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen);

                    if (ImGui::BeginPopupContextItem("TransformMenu")) {
                        if (ImGui::MenuItem("Remover TransformComponent")) {
                            kowai_entity_remove_component(e, COMPONENT_TRANSFORM);
                        }
                        ImGui::EndPopup();
                    }

                    if (open) {
                        if (ImGui::DragFloat3("Pos", t->position, 0.1f)) kowai_transform_update_matrix(t);
                        if (ImGui::DragFloat3("Rot", t->rotation, 1.0f)) kowai_transform_update_matrix(t);
                        if (ImGui::DragFloat3("Scale", t->scale, 0.1f))  kowai_transform_update_matrix(t);
                    }
                    ImGui::Spacing();
                }

                // --- 2. Mesh Render Component ---
                MeshRenderComponent* m = (MeshRenderComponent*)kowai_entity_get_component(e, COMPONENT_MESH_RENDER);
                if (m) {
                    bool open = ImGui::CollapsingHeader("Mesh Render", ImGuiTreeNodeFlags_DefaultOpen);

                    if (ImGui::BeginPopupContextItem("MeshRenderMenu")) {
                        if (ImGui::MenuItem("Remover MeshRenderComponent")) {
                            kowai_entity_remove_component(e, COMPONENT_MESH_RENDER);
                        }
                        ImGui::EndPopup();
                    }

                    if (open) {
                        ImGui::Checkbox("Visible", &m->is_visible);

                        const char* model_id = kowai_asset_bank_get_id_from_model(asset_bank, m->model);
                        char mesh_label[128];
                        SDL_snprintf(mesh_label, sizeof(mesh_label), "Model: %s", m->model ? model_id : "[Ninguno - Suelta un .gltf aquí]");
                        ImGui::Button(mesh_label, ImVec2(-1, 26));

                        // Drop target para assets
                        if (ImGui::BeginDragDropTarget()) {
                            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_FILE_PATH")) {
                                const char* file_path_arrastrado = (const char*)payload->Data;

                                const char* filename = SDL_strrchr(file_path_arrastrado, '/');
                                if (!filename) filename = SDL_strrchr(file_path_arrastrado, '\\');
                                filename = filename ? filename + 1 : file_path_arrastrado;

                                char asset_id[64];
                                SDL_strlcpy(asset_id, filename, sizeof(asset_id));
                                char* dot = SDL_strrchr(asset_id, '.');
                                if (dot) *dot = '\0';

                                KowaiModel* model = kowai_asset_bank_get_model(asset_bank, asset_id);
                                if (!model) {
                                    SDL_GPUDevice* device = kowai_renderer_get_device(kowai_get_renderer(engine));
                                    model = kowai_asset_bank_register_gltf(device, asset_bank, asset_id, file_path_arrastrado);
                                }
                                if (model) {
                                    m->model = model;
                                    m->is_visible = true;
                                }
                            }
                            ImGui::EndDragDropTarget();
                        }
                    }
                    ImGui::Spacing();
                }

                // --- BOTÓN: ADD COMPONENT ---
                ImGui::Separator();
                ImGui::Spacing();

                ImGui::NewLine(); ImGui::SameLine(ImGui::GetWindowWidth() * 0.25f);
                if (ImGui::Button("➕ Add Component", ImVec2(ImGui::GetWindowWidth() * 0.5f, 30))) {
                    ImGui::OpenPopup("AddComponentPopup");
                }

                if (ImGui::BeginPopup("AddComponentPopup")) {
                    ImGui::TextDisabled("Componentes Disponibles");
                    ImGui::Separator();

                    if (!t) {
                        if (ImGui::MenuItem("📍 Transform Component")) {
                            uint32_t id = e->id;
                            scene->transforms[id].position[0] = 0.0f; scene->transforms[id].position[1] = 0.0f; scene->transforms[id].position[2] = 0.0f;
                            scene->transforms[id].rotation[0] = 0.0f; scene->transforms[id].rotation[1] = 0.0f; scene->transforms[id].rotation[2] = 0.0f;
                            scene->transforms[id].scale[0] = 1.0f;    scene->transforms[id].scale[1] = 1.0f;    scene->transforms[id].scale[2] = 1.0f;
                            kowai_transform_update_matrix(&scene->transforms[id]);

                            kowai_entity_add_component(e, COMPONENT_TRANSFORM, &scene->transforms[id]);
                        }
                    }

                    if (!m) {
                        if (ImGui::MenuItem("🗿 Mesh Render Component")) {
                            uint32_t id = e->id;
                            scene->meshes[id].model = NULL;
                            scene->meshes[id].is_visible = false;

                            kowai_entity_add_component(e, COMPONENT_MESH_RENDER, &scene->meshes[id]);
                        }
                    }

                    ImGui::EndPopup();
                }
            }
            ImGui::EndChild();
        }
        else {
            ImGui::Text("Crea o carga una escena desde el menú 'Archivo' para inspeccionar.");
        }
        ImGui::End();

        ImGui::Render();
    }

    void imgui_backend_draw_asset_browser(KowaiEngine* engine, KowaiAssetBank* asset_bank) {
        ImGui::Begin("Explorador de Recursos (VFS)");

        const char* project_path = kowai_engine_get_project_path(engine);
        char assets_root[512];
        SDL_snprintf(assets_root, sizeof(assets_root), "%s/assets", project_path);

        // Llamada inicial a la raíz del árbol jerárquico
        imgui_backend_render_file_tree(asset_bank, assets_root);

        ImGui::End();
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