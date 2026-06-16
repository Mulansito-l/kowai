#include "KowaiEngine/imgui_backend.h"

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlgpu3.h"
#include <nfd.h>
#include <ImGuizmo.h>

extern "C" {

    #include "KowaiEngine/engine.h"
    #include "KowaiEngine/scene.h"
    #include "KowaiEngine/asset_bank.h"
    #include "KowaiEngine/entity.h"
    #include "KowaiEngine/renderer.h"
    #include "KowaiEngine/camera.h"
    #include "KowaiEngine/input.h"

    struct KowaiRenderer* kowai_get_renderer(KowaiEngine* engine);
    static void imgui_backend_render_file_tree(KowaiAssetBank* asset_bank, const char* current_dir_path);
    void kowai_entity_remove_component(KowaiEntity* entity, KowaiComponentType type);
    void imgui_backend_draw_asset_browser(KowaiEngine* engine, KowaiAssetBank* asset_bank);
    void imgui_backend_draw_hierarchy_node(KowaiScene* scene, KowaiEntity* entity, int* seleccionado_idx);

    static KowaiInputActionMap* g_waiting_binding = nullptr;

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

                    // 🟢 DRAG SOURCE OPTIMIZADO PARA CARGA BAJO DEMANDA
                    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                        // Ahora enviamos la ruta del archivo físico, igual que con los modelos
                        ImGui::SetDragDropPayload("DND_FILE_PATH", full_path, SDL_strlen(full_path) + 1);

                        ImGui::Text("Preparando textura: %s", name);
                        ImGui::TextDisabled("(Se cargará en VRAM al soltar)");
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

        // Estado local para abrir la ventana de ajustes globales
        static bool mostrar_ajustes_escena = false;
        static bool mostrar_input_settings = false;
        static bool abrir_popup_nuevo_contexto = false;
        static char nuevo_contexto_nombre[32] = "";
        static bool abrir_popup_nuevo_mapping = false;
        static char nuevo_mapping_nombre[32] = "";

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
                        KowaiScene* cargada = kowai_scene_load(outPath, asset_bank, kowai_renderer_get_device(kowai_get_renderer(engine)));
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

                // =====================================================================
                // 💾 OPCIÓN 1: GUARDAR ESCENA (Directo si ya existe, si no, abre diálogo)
                // =====================================================================
                if (ImGui::MenuItem("Guardar Escena", "Ctrl+S", false, scene != NULL)) {
                    if (scene) {
                        // Si la escena ya tiene una ruta asignada (fue cargada o guardada previamente)
                        if (scene->current_filepath[0] != '\0') {
                            kowai_scene_save(scene, asset_bank, scene->current_filepath);
                            SDL_Log("Editor: Escena guardada rápidamente en: %s", scene->current_filepath);
                        }
                        else {
                            // Si es una escena completamente nueva ('Nueva Escena'), disparamos el diálogo obligatoriamente
                            nfdchar_t* outPath = NULL;
                            nfdresult_t result = NFD_SaveDialog("kscene", project_path, &outPath);

                            if (result == NFD_OKAY) {
                                char finalPath[512];
                                ensure_kscene_extension(outPath, finalPath, sizeof(finalPath));

                                kowai_scene_save(scene, asset_bank, finalPath);
                                free(outPath);
                            }
                        }
                    }
                }

                // =====================================================================
                // 💾 OPCIÓN 2: GUARDAR ESCENA COMO... (Siempre fuerza el File Dialog)
                // =====================================================================
                if (ImGui::MenuItem("Guardar Escena Como...", "Ctrl+Shift+S", false, scene != NULL)) {
                    if (scene) {
                        nfdchar_t* outPath = NULL;
                        nfdresult_t result = NFD_SaveDialog("kscene", project_path, &outPath);

                        if (result == NFD_OKAY) {
                            char finalPath[512];
                            ensure_kscene_extension(outPath, finalPath, sizeof(finalPath));

                            kowai_scene_save(scene, asset_bank, finalPath);
                            free(outPath);
                        }
                    }
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Salir del Motor", "Alt+F4")) {

                    // Modificar bandera de cierre aquí si es necesario
                }

                ImGui::EndMenu();
            }

            // --- MENÚ: ESCENA (GAME OBJECTS) ---
            if (ImGui::BeginMenu("Escena", scene != NULL)) {
                if (ImGui::MenuItem("Spawnear Entidad Vacía")) {
                    kowai_scene_create_entity(scene, "Nueva Entidad");
                }
                ImGui::EndMenu();
            }

            // --- 🟢 NUEVO MENÚ: CONFIGURACIÓN (ENTORNO GLOBAL) ---
            if (ImGui::BeginMenu("Scene settings", scene != NULL)) {
                if (ImGui::MenuItem("Ambient/Lighting settings", NULL, mostrar_ajustes_escena)) {
                    mostrar_ajustes_escena = !mostrar_ajustes_escena;
                }
                ImGui::EndMenu();
            }

            // --- 🟢 NUEVO MENÚ: CONFIGURACIÓN (ENTORNO GLOBAL) ---
            if (ImGui::BeginMenu("Input settings")) {
                if (ImGui::MenuItem("Input mappings", NULL, mostrar_input_settings))
                {
                    mostrar_input_settings = !mostrar_input_settings;
                }

                ImGui::EndMenu();
            }

            // --- INDICADOR DE ESCENA ABIERTA EN LA TOP BAR ---
            ImGui::Separator();
            if (scene != NULL) {
                ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), "🎬 Escena Activa: [%s]", scene->name);
                ImGui::TextDisabled("| Entidades: %d", scene->entity_count);
            }
            else {
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "⚠️ Sin Escena Activa (Archivo -> Nueva Escena)");
            }

            // === PLAY CONTROLS (derecha) ===
            float button_width = 30.0f;
            float total_width = button_width * 3 + ImGui::GetStyle().ItemSpacing.x * 2;
            ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - total_width);

            bool is_playing = kowai_engine_get_play_mode() == KOWAI_MODE_PLAYING;
            bool is_paused = kowai_engine_get_play_mode() == KOWAI_MODE_PAUSED;
            bool is_editor = kowai_engine_get_play_mode() == KOWAI_MODE_EDITOR;

            // PLAY
            if (is_playing) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
            if (ImGui::Button(">", ImVec2(button_width, 0)))
            {
                SDL_Log("Boton PLAY presionado, engine: %p, play_mode: %d", engine, engine ? kowai_engine_get_play_mode() : -1);
                if (is_editor)
                    kowai_engine_play(engine);
                else if (is_paused)
                    kowai_engine_play(engine); // reanudar desde pausa
            }
            if (is_playing) ImGui::PopStyleColor();

            ImGui::SameLine();

            // PAUSE
            if (is_paused) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.7f, 0.2f, 1.0f));
            if (ImGui::Button("=", ImVec2(button_width, 0)))
            {
                if (is_playing)
                    kowai_engine_pause(engine);
            }
            if (is_paused) ImGui::PopStyleColor();

            ImGui::SameLine();

            // STOP
            if (ImGui::Button("*", ImVec2(button_width, 0)))
            {
                if (!is_editor)
                    kowai_engine_stop(engine);
            }

            ImGui::EndMainMenuBar();
        }

        // =========================================================================
        // 🟢 NUEVA VENTANA FLOATING: AJUSTES DE ENTORNO (ENVIRONMENT / LIGHTING)
        // =========================================================================
        if (mostrar_ajustes_escena && scene != NULL) {
            ImGui::Begin("⚙️ Ajustes del Entorno", &mostrar_ajustes_escena, ImGuiWindowFlags_AlwaysAutoResize);

            ImGui::SeparatorText("Iluminación Global (Sol)");

            // Asumiendo que tu struct de Scene almacena la dirección del sol. Si no, usa variables static por mientras
            // Nota: El tercer float (intensidad/velocidad del drag) lo dejamos en 0.05f para control fino
            ImGui::DragFloat3("Dirección Sol", scene->sun_direction, 0.02f, -1.0f, 1.0f, "%.2f");
            ImGui::ColorEdit3("Color Sol", scene->sun_color);

            ImGui::Spacing();
            ImGui::SeparatorText("Ambiente Retro");
            ImGui::ColorEdit3("Luz Ambiental", scene->ambient_color);

            // Si usas un color de fondo limpio para el cielo (Clear Color) o quieres simular el Skydome degradado
            ImGui::ColorEdit3("Color del Cielo (Clear)", scene->sky_clear_color);

            ImGui::Spacing();
            ImGui::Separator();
            if (ImGui::Button("Restablecer Valores Por Defecto", ImVec2(-1, 24))) {
                scene->sun_direction[0] = 0.5f;  scene->sun_direction[1] = 1.0f;  scene->sun_direction[2] = 0.3f;
                scene->sun_color[0] = 1.0f;      scene->sun_color[1] = 1.0f;      scene->sun_color[2] = 1.0f;
                scene->ambient_color[0] = 0.2f;  scene->ambient_color[1] = 0.2f;  scene->ambient_color[2] = 0.2f;
                scene->sky_clear_color[0] = 0.1f; scene->sky_clear_color[1] = 0.1f; scene->sky_clear_color[2] = 0.15f;
            }

            ImGui::End();
        }

        if (mostrar_input_settings) {
            KowaiInputSystem* input =
                kowai_engine_get_input_system_ptr(engine);

            if (!input)
                return;

            static int selected_context = 0;
            ImGui::Begin(
                "Input Settings",
                &mostrar_input_settings
            );

            ImGui::BeginChild("ContextList", ImVec2(220, 0), true);

            static int renaming_context = -1;
            static char rename_buffer[32] = "";

            for (int i = 0; i < input->context_count; i++)
            {
                bool selected = (selected_context == i);

                if (renaming_context == i)
                {
                    ImGui::SetKeyboardFocusHere();
                    ImGui::SetNextItemWidth(-FLT_MIN);

                    if (ImGui::InputText(
                        "##rename",
                        rename_buffer,
                        sizeof(rename_buffer),
                        ImGuiInputTextFlags_EnterReturnsTrue |
                        ImGuiInputTextFlags_AutoSelectAll))
                    {
                        if (rename_buffer[0] != '\0')
                            SDL_strlcpy(
                                input->contexts[i].context_name,
                                rename_buffer,
                                sizeof(input->contexts[i].context_name));
                        renaming_context = -1;
                        kowai_input_save(kowai_engine_get_input_system_ptr(engine), kowai_engine_get_input_system_ptr(engine)->project_path);
                    }

                    // Cancelar con Escape o click afuera
                    if (ImGui::IsKeyPressed(ImGuiKey_Escape) ||
                        (!ImGui::IsItemActive() &&
                            ImGui::IsMouseClicked(0)))
                    {
                        renaming_context = -1;
                    }
                }
                else
                {
                    if (ImGui::Selectable(
                        input->contexts[i].context_name,
                        selected))
                    {
                        selected_context = i;
                    }

                    if (ImGui::IsItemHovered() &&
                        ImGui::IsMouseDoubleClicked(0) &&
                        !input->contexts[i].is_read_only)
                    {
                        renaming_context = i;
                        SDL_strlcpy(
                            rename_buffer,
                            input->contexts[i].context_name,
                            sizeof(rename_buffer));
                    }

                    // Click derecho abre menú contextual
                    if (!input->contexts[i].is_read_only &&
                        ImGui::BeginPopupContextItem())
                    {
                        if (ImGui::MenuItem("Eliminar contexto"))
                        {
                            kowai_input_delete_context(input, i);

                            if (selected_context >= input->context_count)
                                selected_context = input->context_count - 1;
                        }
                        ImGui::EndPopup();
                    }
                }
            }

            ImGui::Separator();

            if (ImGui::Button("+ Crear Contexto", ImVec2(-1, 0)))
            {
                char nombre[32];
                SDL_snprintf(
                    nombre,
                    sizeof(nombre),
                    "Contexto %d",
                    input->context_count + 1);

                int idx = kowai_input_create_context(input, nombre);

                if (idx >= 0)
                {
                    selected_context = idx;
                    // Entrar directo a modo renombre
                    renaming_context = idx;
                    SDL_strlcpy(rename_buffer, nombre, sizeof(rename_buffer));
                }
            }

            ImGui::EndChild();

            ImGui::SameLine();

            ImGui::BeginChild(
                "ContextEditor",
                ImVec2(0, 0),
                true
            );

            if (selected_context < input->context_count)
            {
                KowaiInputContext* ctx =
                    &input->contexts[selected_context];
                ImGui::Text(
                    "Contexto: %s",
                    ctx->context_name
                );

                if (ctx->is_read_only)
                {
                    ImGui::TextColored(
                        ImVec4(1, 1, 0, 1),
                        "Contexto protegido del motor"
                    );
                }

                ImGui::Separator();
                ImGui::Text("Mappings");
                if (ctx->is_read_only)
                {
                    ImGui::BeginDisabled();
                }

                if (ImGui::BeginTable(
                    "MappingsTable",
                    5,
                    ImGuiTableFlags_Borders |
                    ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_Resizable))
                {
                    ImGui::TableSetupColumn("Action");
                    ImGui::TableSetupColumn("Type");
                    ImGui::TableSetupColumn("Binding");
                    ImGui::TableSetupColumn("Modifier");
                    ImGui::TableSetupColumn("##del", ImGuiTableColumnFlags_WidthFixed, 24.0f); // ← nueva

                    ImGui::TableHeadersRow();

                    for (int i = 0; i < ctx->mapping_count; i++)
                    {
                        KowaiInputActionMap* map =
                            &ctx->mappings[i];

                        ImGui::TableNextRow();

                        // ACTION
                        ImGui::TableSetColumnIndex(0);

                        ImGui::PushID(i);

                        ImGui::SetNextItemWidth(-FLT_MIN);

                        char id[64];
                        SDL_snprintf(
                            id,
                            sizeof(id),
                            "##action_%d",
                            i
                        );

                        ImGui::InputText(
                            id,
                            map->action_name,
                            sizeof(map->action_name)
                        );

                        if (ImGui::IsItemDeactivatedAfterEdit())
                        {
                            kowai_input_save(input, input->project_path);
                        }

                        ImGui::PopID();

                        // TYPE
                        ImGui::TableSetColumnIndex(1);

                        ImGui::PushID(i);

                        const char* tipos[] =
                        {
                            "Keyboard",
                            "MouseButton",
                            "MouseWheel",
                            "Gamepad"
                        };

                        int current_type = (int)map->type;

                        ImGui::SetNextItemWidth(-FLT_MIN);

                        if (ImGui::Combo(
                            "##Type",
                            &current_type,
                            tipos,
                            IM_ARRAYSIZE(tipos)))
                        {
                            map->type = (KowaiInputType)current_type;
                        }

                        ImGui::PopID();

                        // BINDING
                        ImGui::TableSetColumnIndex(2);

                        ImGui::PushID(i);

                        const char* label =
                            (g_waiting_binding == map)
                            ? "Press key or mouse button..."
                            : kowai_input_code_to_string(
                                map->type,
                                map->code);

                        if (ImGui::Button(
                            label,
                            ImVec2(-FLT_MIN, 0)))
                        {
                            g_waiting_binding = map;
                        }

                        ImGui::PopID();

                        // MODIFIER
                        ImGui::TableSetColumnIndex(3);

                        if (map->modifier != 0)
                        {
                            ImGui::Text(
                                "%s",
                                kowai_input_modifier_to_string(
                                    map->modifier));
                        }
                        else
                        {
                            ImGui::TextDisabled("-");
                        }

                        ImGui::TableSetColumnIndex(4);
                        ImGui::PushID(i);
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                        if (ImGui::SmallButton("✕"))
                        {
                            kowai_input_delete_mapping(input, ctx, i);
                            ImGui::PopStyleColor();
                            ImGui::PopID();
                            break; // salir del loop para evitar acceso inválido
                        }
                        ImGui::PopStyleColor();
                        ImGui::PopID();
                    }

                    ImGui::EndTable();
                }

                ImGui::Separator();

                static char nuevo_mapping_nombre[32] = "";

                if (ImGui::Button("+ Nuevo Mapping"))
                {
                    strcpy(
                        nuevo_mapping_nombre,
                        "NuevaAccion"
                    );

                    kowai_input_add_mapping(
                        input,
                        ctx->context_name,
                        nuevo_mapping_nombre,
                        INPUT_TYPE_KEYBOARD,
                        SDL_SCANCODE_UNKNOWN,
                        0
                    );
                }

                if (ctx->is_read_only)
                {
                    ImGui::EndDisabled();
                }
            }
            ImGui::EndChild();
            ImGui::End();
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
        // VENTANA 2 RESTAURADA: EXPLORADOR DE RECURSOS (VFS)
        // =========================================================================
        imgui_backend_draw_asset_browser(engine, asset_bank);

        // --- VENTANA 3: INSPECTOR DE JERARQUÍA Y COMPONENTES ---
        static int seleccionado_idx = -1;
        ImGui::Begin("Kowai Engine - Inspector");

        if (scene != NULL) {
            // -----------------------------------------------------------------
        // COLUMNA IZQUIERDA: JERARQUÍA (SCENE GRAPH ARBOREO)
        // -----------------------------------------------------------------
            ImGui::BeginChild("Hierarchy", ImVec2(200, 0), true);

            for (int i = 0; i < MAX_ENTITIES_PER_SCENE; i++) {
                // Dibujamos solo las raíces vivas
                if (scene->entities[i].name[0] != '\0' && scene->entities[i].parent == NULL) {
                    imgui_backend_draw_hierarchy_node(scene, &scene->entities[i], &seleccionado_idx);
                }
            }

            // 🟢 TRUCO CLAVE: Creamos un espacio invisible que rellena TODO lo que sobra del panel.
            // Esto asegura que haya un área enorme abajo para soltar cosas y des-emparentar.
            ImVec2 espacio_disponible = ImGui::GetContentRegionAvail();
            if (espacio_disponible.y > 0.0f) {
                ImGui::Dummy(espacio_disponible);
            }

            // El Drag and Drop Target ahora se aplica a TODA la ventana, incluyendo el Dummy invisible
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_ENTITY_INDEX")) {
                    int arrastrado_idx = *(const int*)payload->Data;
                    KowaiEntity* hijo = &scene->entities[arrastrado_idx];

                    // Si la soltamos aquí en el vacío y tenía un padre, la liberamos
                    if (hijo->parent != NULL) {
                        kowai_entity_set_parent(hijo, NULL);
                        SDL_Log("Editor: '%s' extraído a la raíz de la escena.", hijo->name);
                    }
                }
                ImGui::EndDragDropTarget();
            }

            ImGui::EndChild();

            ImGui::SameLine();

            // -----------------------------------------------------------------
            // COLUMNA DERECHA: COMPONENTES INDIVIDUALES
            // -----------------------------------------------------------------
            ImGui::BeginChild("Components");
            if (seleccionado_idx != -1 && scene->entities[seleccionado_idx].name[0] != '\0') {
                KowaiEntity* e = &scene->entities[seleccionado_idx];

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
                        if (ImGui::DragFloat3("Pos", t->position, 0.1f)) kowai_transform_update_matrix(e);
                        if (ImGui::DragFloat3("Rot", t->rotation, 1.0f)) kowai_transform_update_matrix(e);
                        if (ImGui::DragFloat3("Scale", t->scale, 0.1f))  kowai_transform_update_matrix(e);
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
                        SDL_snprintf(mesh_label, sizeof(mesh_label), "Model: %s", m->model ? model_id : "[Ninguno - Suelta un .gltf/.glb aquí]");
                        ImGui::Button(mesh_label, ImVec2(-1, 26));

                        if (ImGui::BeginDragDropTarget()) {
                            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_FILE_PATH")) {
                                const char* file_path_arrastrado = (const char*)payload->Data;
                                const char* ext = SDL_strrchr(file_path_arrastrado, '.');

                                if (ext && (SDL_strcasecmp(ext, ".gltf") == 0 ||
                                    SDL_strcasecmp(ext, ".glb") == 0 ||
                                    SDL_strcasecmp(ext, ".obj") == 0))
                                {
                                    const char* filename = SDL_strrchr(file_path_arrastrado, '/');
                                    if (!filename) filename = SDL_strrchr(file_path_arrastrado, '\\');
                                    filename = filename ? filename + 1 : file_path_arrastrado;

                                    char asset_id[64];
                                    SDL_strlcpy(asset_id, filename, sizeof(asset_id));
                                    char* dot = SDL_strrchr(asset_id, '.');
                                    if (dot) *dot = '\0';

                                    SDL_GPUDevice* device = kowai_renderer_get_device(kowai_get_renderer(engine));

                                    KowaiModel* model = kowai_asset_bank_get_model(asset_bank, asset_id);
                                    if (!model) {
                                        model = kowai_asset_bank_register_gltf(device, asset_bank, asset_id, file_path_arrastrado);
                                    }

                                    if (model) {
                                        m->model = model;
                                        m->is_visible = true;
                                        SDL_Log("Editor: Modelo '%s' inyectado con éxito en la entidad.", asset_id);
                                    }
                                }
                            }
                            ImGui::EndDragDropTarget();
                        }

                        if (m->model) {
                            ImGui::Spacing();
                            ImGui::SeparatorText("Sub-Mallas / Texturas");

                            for (Uint32 i = 0; i < m->model->mesh_count; i++) {
                                KowaiMesh* mesh = &m->model->meshes[i];
                                ImGui::PushID(i);

                                char slot_label[128];
                                if (mesh->texture) {
                                    SDL_snprintf(slot_label, sizeof(slot_label), "Malla [%d]: [Asignada - %p]", i, mesh->texture);
                                }
                                else {
                                    SDL_snprintf(slot_label, sizeof(slot_label), "Malla [%d]: [Color Plano / Fallback]", i);
                                }

                                ImGui::Button(slot_label, ImVec2(-1, 24));

                                if (ImGui::BeginDragDropTarget()) {
                                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_FILE_PATH")) {
                                        const char* file_path_arrastrado = (const char*)payload->Data;
                                        const char* ext = SDL_strrchr(file_path_arrastrado, '.');

                                        if (ext && (SDL_strcasecmp(ext, ".png") == 0 || SDL_strcasecmp(ext, ".jpg") == 0)) {
                                            const char* filename = SDL_strrchr(file_path_arrastrado, '/');
                                            if (!filename) filename = SDL_strrchr(file_path_arrastrado, '\\');
                                            filename = filename ? filename + 1 : file_path_arrastrado;

                                            char asset_id[64];
                                            SDL_strlcpy(asset_id, filename, sizeof(asset_id));
                                            char* dot = SDL_strrchr(asset_id, '.');
                                            if (dot) *dot = '\0';

                                            SDL_GPUDevice* device = kowai_renderer_get_device(kowai_get_renderer(engine));

                                            void* texture = kowai_asset_bank_get_texture(asset_bank, asset_id);
                                            if (!texture) {
                                                texture = kowai_asset_bank_register_texture(device, asset_bank, asset_id, file_path_arrastrado);
                                            }

                                            if (texture) {
                                                mesh->texture = (SDL_GPUTexture*)texture;

                                                if (mesh->sampler) {
                                                    SDL_ReleaseGPUSampler(device, mesh->sampler);
                                                }

                                                SDL_GPUSamplerCreateInfo samplerInfo{};
                                                samplerInfo.min_filter = SDL_GPU_FILTER_NEAREST;
                                                samplerInfo.mag_filter = SDL_GPU_FILTER_NEAREST;
                                                samplerInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
                                                samplerInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
                                                samplerInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
                                                samplerInfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
                                                mesh->sampler = SDL_CreateGPUSampler(device, &samplerInfo);

                                                SDL_Log("Editor: Textura inyectada en sub-malla index [%d].", i);
                                            }
                                        }
                                    }
                                    ImGui::EndDragDropTarget();
                                }
                                ImGui::PopID();
                            }
                        }
                    }
                    ImGui::Spacing();
                }

                // --- 🟢 3. NUEVO: Light Component ---
                LightComponent* l = (LightComponent*)kowai_entity_get_component(e, COMPONENT_LIGHT);
                if (l) {
                    bool open = ImGui::CollapsingHeader("💡 Light Component (Point Light)", ImGuiTreeNodeFlags_DefaultOpen);

                    // Menú contextual para remover el componente con click derecho
                    if (ImGui::BeginPopupContextItem("LightMenu")) {
                        if (ImGui::MenuItem("Remover LightComponent")) {
                            kowai_entity_remove_component(e, COMPONENT_LIGHT);
                        }
                        ImGui::EndPopup();
                    }

                    if (open) {
                        ImGui::ColorEdit3("Color Luz", l->color);
                        ImGui::DragFloat("Intensidad", &l->intensity, 0.05f, 0.0f, 20.0f, "%.2f");
                        ImGui::DragFloat("Radio de Alcance", &l->radius, 0.1f, 0.1f, 100.0f, "%.1f u");
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
                            kowai_transform_update_matrix(&scene->entities[id]);
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

                    // 🟢 NUEVA OPCIÓN: Agregar Light Component si la entidad no tiene uno ya asignado
                    if (!l) {
                        if (ImGui::MenuItem("💡 Light Component")) {
                            uint32_t id = e->id;
                            // Inicialización con valores retro estándar funcionales
                            scene->lights[id].color[0] = 1.0f;
                            scene->lights[id].color[1] = 1.0f;
                            scene->lights[id].color[2] = 0.8f; // Blanco cálido por defecto
                            scene->lights[id].intensity = 1.0f;
                            scene->lights[id].radius = 10.0f;

                            kowai_entity_add_component(e, COMPONENT_LIGHT, &scene->lights[id]);
                            SDL_Log("Editor: LightComponent acoplado a la entidad '%s' (ID: %d)", e->name, id);
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

        if (scene != NULL && seleccionado_idx != -1 && scene->entities[seleccionado_idx].name[0] != '\0') {
            KowaiEntity* e = &scene->entities[seleccionado_idx];
            TransformComponent* t = (TransformComponent*)kowai_entity_get_component(e, COMPONENT_TRANSFORM);

            // Solo podemos transformar cosas que tengan un componente Transform asignado
            if (t) {
                // 1. Inicializar ImGuizmo para el frame actual
                ImGuizmo::BeginFrame();

                // Configuramos que se dibuje en toda la pantalla (o puedes limitarlo al tamaño de tu render)
                ImGuiIO& io = ImGui::GetIO();
                ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);

                // 2. Extraer las matrices de tu cámara y motor (cglm usa arreglos planos compatibles)
                // Nota: Asegúrate de tener getters o acceso a estas matrices desde engine->camera
                // Si tus matrices están en formato de cglm (mat4), se pueden pasar directo como (float*)
                float* view_matrix = (float*)kowai_engine_get_active_camera(engine)->view_matrix;
                float* proj_matrix = (float*)kowai_engine_get_active_camera(engine)->proj_matrix;

                // Matriz del objeto actual que vamos a manipular
                float* model_matrix = (float*)t->global_matrix;

                // 3. Atajos de teclado estilo Unity para cambiar de herramienta (Opcional pero muy cómodo)
                static ImGuizmo::OPERATION operacion_actual = ImGuizmo::TRANSLATE;

                KowaiInputSystem* input = kowai_engine_get_input_system_ptr(engine);

                if (kowai_input_get_action_down(input, "TranslateMode")) {
                    operacion_actual = ImGuizmo::TRANSLATE;
                }

                if (kowai_input_get_action_down(input, "RotateMode")) {
                    // Puedes exponer una pequeña función en imgui_backend para cambiar el estado del gizmo, ej:
                    operacion_actual = ImGuizmo::ROTATE;
                }

                if (kowai_input_get_action_down(input, "ScaleMode")) {
                    // Puedes exponer una pequeña función en imgui_backend para cambiar el estado del gizmo, ej:
                    operacion_actual = ImGuizmo::SCALE;
                }

                // 4. Dibujar e interactuar con el Gizmo
                // Usamos la coordenada LOCAL para que las flechas giren junto al objeto, o WORLD para ejes fijos
                if (ImGuizmo::Manipulate(view_matrix, proj_matrix, operacion_actual, ImGuizmo::LOCAL, model_matrix)) {

                    // 5. SI EL USUARIO ARRASTRÓ EL GIZMO:
                    // Descomponemos la nueva matriz modificada de vuelta a variables independientes de posición, rotación y escala
                    float nueva_pos[3], nueva_rot[3], nueva_scale[3];
                    ImGuizmo::DecomposeMatrixToComponents(model_matrix, nueva_pos, nueva_rot, nueva_scale);

                    // Copiamos los valores de vuelta a tu componente Transform para que el render lo note
                    for (int i = 0; i < 3; i++) {
                        t->position[i] = nueva_pos[i];
                        t->rotation[i] = nueva_rot[i];
                        t->scale[i] = nueva_scale[i];
                    }

                    // Forzamos la actualización interna de las matrices normales y el modelo
                    kowai_transform_update_matrix(e);
                }
            }
        }

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

    void imgui_backend_draw_hierarchy_node(KowaiScene* scene, KowaiEntity* entity, int* seleccionado_idx)
    {
        int actual_idx = (int)(entity - scene->entities);

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;

        if (*seleccionado_idx == actual_idx) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        // Determinar si es una hoja ANTES de crear el nodo
        bool es_hoja = (entity->first_child == NULL);
        if (es_hoja) {
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        }

        bool nodo_abierto = ImGui::TreeNodeEx((void*)(intptr_t)actual_idx, flags, entity->name);

        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
            *seleccionado_idx = actual_idx;
        }

        // -----------------------------------------------------------------
        // DRAG AND DROP SOURCE
        // -----------------------------------------------------------------
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
            ImGui::SetDragDropPayload("DND_ENTITY_INDEX", &actual_idx, sizeof(int));
            ImGui::Text("Mover entidad: %s", entity->name);
            ImGui::EndDragDropSource();
        }

        // -----------------------------------------------------------------
        // DRAG AND DROP TARGET
        // -----------------------------------------------------------------
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_ENTITY_INDEX")) {
                int arrastrado_idx = *(const int*)payload->Data;

                if (arrastrado_idx != actual_idx) {
                    KowaiEntity* hijo = &scene->entities[arrastrado_idx];

                    bool es_ancestro = false;
                    KowaiEntity* check = entity->parent;
                    while (check != NULL) {
                        if (check == hijo) {
                            es_ancestro = true;
                            break;
                        }
                        check = check->parent;
                    }

                    if (!es_ancestro) {
                        kowai_entity_set_parent(hijo, entity);
                        SDL_Log("Editor: '%s' es ahora hijo de '%s'", hijo->name, entity->name);
                    }
                    else {
                        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Editor: Ciclo jerárquico evitado.");
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }

        // Menú contextual
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("❌ Eliminar Entidad")) {
                SDL_Log("ECS: Destruyendo entidad '%s' (ID: %d)", entity->name, actual_idx);
                kowai_scene_destroy_entity(scene, entity->id);
                if (*seleccionado_idx == actual_idx) {
                    *seleccionado_idx = -1;
                }
            }
            ImGui::EndPopup();
        }

        // 🟢 SOLUCIÓN AL CRASH: 
        // Solo renderizamos hijos y hacemos TreePop si el nodo está abierto Y NO era una hoja que activó NoTreePushOnOpen
        if (nodo_abierto && !es_hoja) {
            KowaiEntity* hijo = entity->first_child;
            while (hijo != NULL) {
                imgui_backend_draw_hierarchy_node(scene, hijo, seleccionado_idx);
                hijo = hijo->next_sibling;
            }
            ImGui::TreePop(); // Ahora solo se llamará si ImGui realmente hizo un Push
        }
    }

    void imgui_backend_prepare(SDL_GPUCommandBuffer* cmd) {
        ImGui_ImplSDLGPU3_PrepareDrawData(ImGui::GetDrawData(), cmd);
    }

    void imgui_backend_render(SDL_GPUCommandBuffer* cmd, SDL_GPURenderPass* pass) {
        ImGui_ImplSDLGPU3_RenderDrawData(ImGui::GetDrawData(), cmd, pass, nullptr);
    }

    bool imgui_backend_process_event(SDL_Event* event, KowaiInputSystem* input) {
        ImGuiIO& io = ImGui::GetIO();

        bool handled = ImGui_ImplSDL3_ProcessEvent(event);


        if (event->type == SDL_EVENT_GAMEPAD_ADDED)
        {
            kowai_input_on_gamepad_added(input, event->gdevice.which);
        }

        // Siempre procesar el wheel, fuera del g_waiting_binding
        if (event->type == SDL_EVENT_MOUSE_WHEEL)
        {
            if (event->wheel.y > 0)
                input->current_wheel_up = true;
            else if (event->wheel.y < 0)
                input->current_wheel_down = true;
        }

        if (g_waiting_binding)
        {
            if (event->type == SDL_EVENT_KEY_DOWN)
            {
                SDL_Scancode scancode = event->key.scancode;
                SDL_Keymod mods = event->key.mod;

                g_waiting_binding->modifier = 0;
                if (mods & SDL_KMOD_CTRL)
                    g_waiting_binding->modifier |= INPUT_MOD_CTRL;
                if (mods & SDL_KMOD_SHIFT)
                    g_waiting_binding->modifier |= INPUT_MOD_SHIFT;
                if (mods & SDL_KMOD_ALT)
                    g_waiting_binding->modifier |= INPUT_MOD_ALT;

                if (scancode == SDL_SCANCODE_ESCAPE)
                {
                    g_waiting_binding = nullptr;
                    return true;
                }

                if (scancode == SDL_SCANCODE_LCTRL ||
                    scancode == SDL_SCANCODE_RCTRL ||
                    scancode == SDL_SCANCODE_LSHIFT ||
                    scancode == SDL_SCANCODE_RSHIFT ||
                    scancode == SDL_SCANCODE_LALT ||
                    scancode == SDL_SCANCODE_RALT)
                {
                    return true;
                }

                g_waiting_binding->type = INPUT_TYPE_KEYBOARD;
                g_waiting_binding->code = scancode;
                g_waiting_binding = nullptr;
                kowai_input_save(input, input->project_path);
                return true;
            }

            // ← nuevo: captura de botón de mouse
            if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN)
            {
                g_waiting_binding->modifier = 0; // mouse buttons no usan modifier
                g_waiting_binding->type = INPUT_TYPE_MOUSE_BUTTON;
                g_waiting_binding->code = event->button.button;
                g_waiting_binding = nullptr;
                kowai_input_save(input, input->project_path);
                return true;
            }

            if (event->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN)
            {
                g_waiting_binding->modifier = 0;
                g_waiting_binding->type = INPUT_TYPE_GAMEPAD_BUTTON;
                g_waiting_binding->code = event->gbutton.button;
                g_waiting_binding = nullptr;
                kowai_input_save(input, input->project_path);
                return true;
            }

            // Dentro de g_waiting_binding:
            if (event->type == SDL_EVENT_MOUSE_WHEEL)
            {
                g_waiting_binding->modifier = 0;
                g_waiting_binding->type = INPUT_TYPE_MOUSE_WHEEL;
                g_waiting_binding->code = (event->wheel.y > 0)
                    ? KOWAI_WHEEL_UP
                    : KOWAI_WHEEL_DOWN;
                g_waiting_binding = nullptr;
                kowai_input_save(input, input->project_path);
                return true;
            }
        }

        return false;
    }

    void imgui_backend_skip_frame(void) {
        ImGui::EndFrame();
    }

} // extern "C"