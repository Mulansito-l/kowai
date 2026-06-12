#include "KowaiEngine/renderer.h"
#include "KowaiEngine/types.h"
#include "KowaiEngine/entity.h"
#include "KowaiEngine/camera.h"
#include "KowaiEngine/model.h"
#include "KowaiEngine/imgui_backend.h"
#include "KowaiEngine/scene.h"

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h> // Para malloc y free
#include <KowaiEngine/cglm/cglm.h>

#define WIDTH 1280
#define HEIGHT 720

// Prototipo de la función estática para que compile sin problemas de orden
static SDL_GPUShader* kowai_load_shader(
    SDL_GPUDevice* gpu_device,
    const char* filepath,
    SDL_GPUShaderStage stage,
    Uint32 num_samplers,
    Uint32 num_storage_textures,
    Uint32 num_storage_buffers,
    Uint32 num_uniform_buffers);

struct KowaiRenderer {
    SDL_GPUDevice* gpu_device;
    SDL_GPUCommandBuffer* current_cmd_buffer;
    SDL_GPUTexture* current_swapchain_texture;
    SDL_GPUTexture* depth_texture;
    SDL_GPURenderPass* current_render_pass;

    int current_w;
    int current_h;

    // Almacenamiento de Shaders
    SDL_GPUShader* vertex_shader;
    SDL_GPUShader* fragment_shader;

    // Objeto del Pipeline Gráfico
    SDL_GPUGraphicsPipeline* pipeline;

    // Texturas Fallback
    SDL_GPUTexture* default_texture;
    SDL_GPUSampler* default_sampler;

    float current_clear_color[3];
    float current_sun_dir[3];
    float current_sun_color[3];
    float current_ambient_color[3];
};

typedef struct {
    float position_and_active[4]; // .xyz = position, .w = is_active (1.0f o 0.0f)
    float color_and_radius[4];    // .xyz = color * intensity, .w = radius
} GpuPointLight;

typedef struct {
    mat4 model;
    mat4 view;
    mat4 projection;
    mat4 normal_matrix;
    vec4 sun_direction;   // .xyz = dir,   .w = padding
    vec4 sun_color;       // .xyz = color, .w = padding
    vec4 ambient_color;   // .xyz = color, .w = padding
    GpuPointLight lights[MAX_LIGHTS_PER_OBJECT];
} KowaiMVP;

KowaiRenderer* kowai_renderer_create(SDL_Window* window) {
    KowaiRenderer* renderer = (KowaiRenderer*)malloc(sizeof(KowaiRenderer));
    if (!renderer) return NULL;

    // 1. Inicializar dispositivo GPU
    renderer->gpu_device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, NULL);
    if (!renderer->gpu_device) {
        free(renderer);
        return NULL;
    }

    if (!SDL_ClaimWindowForGPUDevice(renderer->gpu_device, window)) {
        SDL_Log("Renderer Error: No se pudo reclamar la ventana: %s", SDL_GetError());
    }

    // Inicializar punteros de estado por seguridad
    renderer->current_cmd_buffer = NULL;
    renderer->current_swapchain_texture = NULL;
    renderer->current_render_pass = NULL;
    renderer->current_w = WIDTH;
    renderer->current_h = HEIGHT;

    // 2. Cargar Shaders estándar
    renderer->vertex_shader = kowai_load_shader(renderer->gpu_device, "D:\\Proyectos\\Kowai\\KowaiTest\\shaders\\triangle.vert.spv", SDL_GPU_SHADERSTAGE_VERTEX, 0, 0, 0, 1);
    renderer->fragment_shader = kowai_load_shader(renderer->gpu_device, "D:\\Proyectos\\Kowai\\KowaiTest\\shaders\\triangle.frag.spv", SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 0, 0, 0);

    if (renderer->vertex_shader && renderer->fragment_shader) {
        SDL_Log("KowaiRenderer: Shaders del sistema cargados con éxito.");
    }
    else {
        SDL_Log("KowaiRenderer Warning: Error crítico al cargar los shaders del sistema.");
    }

    // 3. Crear el Pipeline Gráfico inmutable
    if (renderer->vertex_shader && renderer->fragment_shader) {
        SDL_GPUVertexBufferDescription vertex_buffer_desc = {
            .slot = 0,
            .pitch = sizeof(KowaiVertex),
            .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX
        };

        SDL_GPUVertexAttribute vertex_attributes[3] = {
            {.location = 0, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = (Uint32)offsetof(KowaiVertex, x) },
            {.location = 1, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = (Uint32)offsetof(KowaiVertex, nx) },
            {.location = 2, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, .offset = (Uint32)offsetof(KowaiVertex, u) }
        };

        SDL_GPUGraphicsPipelineCreateInfo pipeline_info = {
            .vertex_shader = renderer->vertex_shader,
            .fragment_shader = renderer->fragment_shader,
            .vertex_input_state = {
                .vertex_buffer_descriptions = &vertex_buffer_desc,
                .num_vertex_buffers = 1,
                .vertex_attributes = vertex_attributes,
                .num_vertex_attributes = 3,
            },
            .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
            .depth_stencil_state = {
                .enable_depth_test = true,
                .enable_depth_write = true,
                .compare_op = SDL_GPU_COMPAREOP_LESS
            },
            .target_info = {
                .color_target_descriptions = (SDL_GPUColorTargetDescription[]){{
                    .format = SDL_GetGPUSwapchainTextureFormat(renderer->gpu_device, window),
                    .blend_state = {
                        .src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
                        .dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
                        .color_blend_op = SDL_GPU_BLENDOP_ADD,
                        .src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
                        .dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO,
                        .alpha_blend_op = SDL_GPU_BLENDOP_ADD,
                        .enable_blend = true
                    }
                }},
                .num_color_targets = 1,
                .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
                .has_depth_stencil_target = true
            }
        };

        renderer->pipeline = SDL_CreateGPUGraphicsPipeline(renderer->gpu_device, &pipeline_info);

        if (renderer->pipeline) {
            SDL_Log("KowaiRenderer: Pipeline gráfico inmutable creado con éxito.");
        }
        else {
            SDL_Log("KowaiRenderer Error: No se pudo crear el Pipeline gráfico: %s", SDL_GetError());
        }
    }
    else {
        renderer->pipeline = NULL;
    }

    // 4. Crear textura de profundidad inicial
    int fb_width, fb_height;
    SDL_GetWindowSizeInPixels(window, &fb_width, &fb_height);

    SDL_GPUTextureCreateInfo depth_info = {
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
        .width = (Uint32)fb_width,
        .height = (Uint32)fb_height,
        .layer_count_or_depth = 1,
        .num_levels = 1,
        .usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET
    };
    renderer->depth_texture = SDL_CreateGPUTexture(renderer->gpu_device, &depth_info);

    // 5. Creación de textura fallback por defecto (Blanco Puro 2x2)
    Uint8 white_pixels[] = {
        255, 255, 255, 255,  255, 255, 255, 255,
        255, 255, 255, 255,  255, 255, 255, 255
    };
    Uint32 texture_size = 2 * 2 * 4;

    SDL_GPUTextureCreateInfo default_tex_info = {
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        .width = 2, .height = 2, .layer_count_or_depth = 1,
        .num_levels = 1, .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER
    };
    renderer->default_texture = SDL_CreateGPUTexture(renderer->gpu_device, &default_tex_info);

    SDL_GPUTransferBufferCreateInfo default_tb_info = {
        .size = texture_size,
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD
    };
    SDL_GPUTransferBuffer* default_tb = SDL_CreateGPUTransferBuffer(renderer->gpu_device, &default_tb_info);

    void* tex_data = SDL_MapGPUTransferBuffer(renderer->gpu_device, default_tb, false);
    SDL_memcpy(tex_data, white_pixels, texture_size);
    SDL_UnmapGPUTransferBuffer(renderer->gpu_device, default_tb);

    // Envío aislado y seguro a la GPU antes de que exista cualquier render pass
    SDL_GPUCommandBuffer* tex_cmd = SDL_AcquireGPUCommandBuffer(renderer->gpu_device);
    SDL_GPUCopyPass* tex_copy = SDL_BeginGPUCopyPass(tex_cmd);
    SDL_GPUTextureTransferInfo tex_src = { .transfer_buffer = default_tb, .offset = 0 };
    SDL_GPUTextureRegion tex_dst = { .texture = renderer->default_texture, .w = 2, .h = 2, .d = 1 };

    SDL_UploadToGPUTexture(tex_copy, &tex_src, &tex_dst, false);
    SDL_EndGPUCopyPass(tex_copy);
    SDL_SubmitGPUCommandBuffer(tex_cmd);
    SDL_ReleaseGPUTransferBuffer(renderer->gpu_device, default_tb);

    SDL_GPUSamplerCreateInfo default_samp_info = {
        .min_filter = SDL_GPU_FILTER_LINEAR,
        .mag_filter = SDL_GPU_FILTER_LINEAR,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT
    };
    renderer->default_sampler = SDL_CreateGPUSampler(renderer->gpu_device, &default_samp_info);
    SDL_Log("KowaiRenderer: Textura fallback por defecto creada.");

    // 🟢 Valores de entorno por defecto (por si no hay escena activa al arrancar)
    glm_vec3_copy((vec3) { 0.1f, 0.1f, 0.15f }, renderer->current_clear_color);
    glm_vec3_copy((vec3) { 0.5f, 1.0f, 0.3f }, renderer->current_sun_dir);
    glm_vec3_copy((vec3) { 1.0f, 1.0f, 1.0f }, renderer->current_sun_color);
    glm_vec3_copy((vec3) { 0.2f, 0.2f, 0.25f }, renderer->current_ambient_color);

    return renderer;
}

bool kowai_renderer_begin_frame(KowaiRenderer* renderer, SDL_Window* window)
{
    renderer->current_cmd_buffer =
        SDL_AcquireGPUCommandBuffer(renderer->gpu_device);

    if (!renderer->current_cmd_buffer)
        return false;

    Uint32 w = 0;
    Uint32 h = 0;

    if (!SDL_WaitAndAcquireGPUSwapchainTexture(
        renderer->current_cmd_buffer,
        window,
        &renderer->current_swapchain_texture,
        &w,
        &h))
    {
        SDL_SubmitGPUCommandBuffer(renderer->current_cmd_buffer);
        return false;
    }

    if ((int)w != renderer->current_w ||
        (int)h != renderer->current_h)
    {
        if (renderer->depth_texture)
        {
            SDL_ReleaseGPUTexture(
                renderer->gpu_device,
                renderer->depth_texture);
        }

        SDL_GPUTextureCreateInfo depth_info = {
            .type = SDL_GPU_TEXTURETYPE_2D,
            .format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
            .width = w,
            .height = h,
            .layer_count_or_depth = 1,
            .num_levels = 1,
            .usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET
        };

        renderer->depth_texture =
            SDL_CreateGPUTexture(
                renderer->gpu_device,
                &depth_info);

        renderer->current_w = (int)w;
        renderer->current_h = (int)h;
    }

    imgui_backend_new_frame();

    return true;
}

void kowai_renderer_begin_render_pass_3d(KowaiRenderer* renderer)
{
    // 🟢 Inicializar con {0} limpia absolutamente todo (layer_or_depth_index, mip_level, etc.)
    SDL_GPUColorTargetInfo color_target = { 0 };

    color_target.texture = renderer->current_swapchain_texture;
    // Eliminadas las asignaciones manuales que daban error; {0} ya hace el trabajo por ti

    color_target.clear_color.r = renderer->current_clear_color[0];
    color_target.clear_color.g = renderer->current_clear_color[1];
    color_target.clear_color.b = renderer->current_clear_color[2];
    color_target.clear_color.a = 1.0f;
    color_target.load_op = SDL_GPU_LOADOP_CLEAR;
    color_target.store_op = SDL_GPU_STOREOP_STORE;

    // 🟢 Inicializar con {0} también limpia los índices internos aquí
    SDL_GPUDepthStencilTargetInfo depth_target = { 0 };
    depth_target.texture = renderer->depth_texture;
    depth_target.clear_depth = 1.0f;
    depth_target.load_op = SDL_GPU_LOADOP_CLEAR;
    depth_target.store_op = SDL_GPU_STOREOP_STORE;

    renderer->current_render_pass =
        SDL_BeginGPURenderPass(
            renderer->current_cmd_buffer,
            &color_target,
            1,
            &depth_target);

    SDL_BindGPUGraphicsPipeline(
        renderer->current_render_pass,
        renderer->pipeline);
}

void kowai_renderer_begin_render_pass_ui(KowaiRenderer* renderer)
{
    SDL_GPUColorTargetInfo color_target = {
        .texture = renderer->current_swapchain_texture,

        // MUY IMPORTANTE
        .load_op = SDL_GPU_LOADOP_LOAD,

        .store_op = SDL_GPU_STOREOP_STORE
    };

    renderer->current_render_pass =
        SDL_BeginGPURenderPass(
            renderer->current_cmd_buffer,
            &color_target,
            1,
            NULL); // <- sin depth
}

void kowai_renderer_end_render_pass(KowaiRenderer* renderer)
{
    if (renderer && renderer->current_render_pass)
    {
        SDL_EndGPURenderPass(renderer->current_render_pass);
        renderer->current_render_pass = NULL;
    }
}

void kowai_renderer_end_frame(KowaiRenderer* renderer)
{
    if (!renderer)
        return;

    if (renderer->current_render_pass)
    {
        SDL_EndGPURenderPass(
            renderer->current_render_pass);
    }

    if (renderer->current_cmd_buffer)
    {
        SDL_SubmitGPUCommandBuffer(
            renderer->current_cmd_buffer);
    }

    renderer->current_render_pass = NULL;
    renderer->current_cmd_buffer = NULL;
    renderer->current_swapchain_texture = NULL;
}

void kowai_renderer_draw_model(
    KowaiRenderer* renderer,
    KowaiModel* model,
    mat4 model_matrix,
    KowaiCamera* camera,
    struct KowaiScene* scene) {
    if (!renderer || !renderer->current_render_pass || !model || !renderer->pipeline || !camera) return;

    // 🟢 CALCULAR LA MATRIZ DE NORMALES EN CPU (Una vez por modelo antes del bucle de mallas)
    mat3 upper_left;
    mat3 inv_matrix;
    mat3 normal_matrix_3x3;

    // 🟢 EXTRACCIÓN MANUAL: Reemplaza por completo glm_mat4_copy33 o pick33
    // Copiamos la esquina superior izquierda 3x3 de model_matrix a upper_left
    glm_vec3_copy(model_matrix[0], upper_left[0]);
    glm_vec3_copy(model_matrix[1], upper_left[1]);
    glm_vec3_copy(model_matrix[2], upper_left[2]);

    // 2. Inversa (Esta sí te compila bien porque es void inline)
    glm_mat3_inv(upper_left, inv_matrix);

    // 3. Transpuesta directo al destino final
    glm_mat3_transpose_to(inv_matrix, normal_matrix_3x3);

    // Preparar el bloque MVP alineado para la GPU...
    KowaiMVP mvp;
    glm_mat4_copy(model_matrix, mvp.model);
    glm_mat4_copy(camera->view_matrix, mvp.view);
    glm_mat4_copy(camera->proj_matrix, mvp.projection);
    glm_mat4_identity(mvp.normal_matrix);
    glm_vec3_copy(normal_matrix_3x3[0], mvp.normal_matrix[0]);
    glm_vec3_copy(normal_matrix_3x3[1], mvp.normal_matrix[1]);
    glm_vec3_copy(normal_matrix_3x3[2], mvp.normal_matrix[2]);

    // Pasar datos de entorno global
    glm_vec3_copy(renderer->current_sun_dir, mvp.sun_direction);
    mvp.sun_direction[3] = 0.0f;
    glm_vec3_copy(renderer->current_sun_color, mvp.sun_color);
    mvp.sun_color[3] = 1.0f;
    glm_vec3_copy(renderer->current_ambient_color, mvp.ambient_color);
    mvp.ambient_color[3] = 1.0f;

    // 🟢 RECOLECCIÓN DE LUCES DINÁMICAS LOCALES
    // Inicializar el arreglo del UBO como inactivo
    for (int l = 0; l < MAX_LIGHTS_PER_OBJECT; l++) {
        mvp.lights[l].position_and_active[3] = 0.0f; // isActive = 0.0f
    }

    if (scene) {
        int lights_found = 0;
        vec3 mesh_pos = { model_matrix[3][0], model_matrix[3][1], model_matrix[3][2] };

        // Iterar el pool de entidades de la escena que pasamos por parámetro
        for (int e = 0; e < MAX_ENTITIES_PER_SCENE && lights_found < MAX_LIGHTS_PER_OBJECT; e++) {
            KowaiEntity* light_ent = &scene->entities[e];

            // Validar si la entidad existe/está activa en tu ECS
            if (light_ent->id == 0) continue;

            // Extraer componentes usando tus funciones del ECS
            LightComponent* light = (LightComponent*)kowai_entity_get_component(light_ent, COMPONENT_LIGHT);
            TransformComponent* light_trans = (TransformComponent*)kowai_entity_get_component(light_ent, COMPONENT_TRANSFORM);

            if (light && light_trans) {
                // Obtener posición del transform de la luz (columna 3 de su matriz de modelo)
                vec3 light_pos = { light_trans->global_matrix[3][0], light_trans->global_matrix[3][1], light_trans->global_matrix[3][2] };

                // Filtro por distancia: ¿La luz alcanza a tocar el centro de este objeto?
                float dist = glm_vec3_distance(mesh_pos, light_pos);
                if (dist <= light->radius) {
                    int idx = lights_found;

                    // Empaquetar Posición y Estado Activo
                    mvp.lights[idx].position_and_active[0] = light_pos[0];
                    mvp.lights[idx].position_and_active[1] = light_pos[1];
                    mvp.lights[idx].position_and_active[2] = light_pos[2];
                    mvp.lights[idx].position_and_active[3] = 1.0f; // 🟢 ¡Encendida!

                    // Empaquetar Color (con intensidad aplicada) y Radio
                    mvp.lights[idx].color_and_radius[0] = light->color[0] * light->intensity;
                    mvp.lights[idx].color_and_radius[1] = light->color[1] * light->intensity;
                    mvp.lights[idx].color_and_radius[2] = light->color[2] * light->intensity;
                    mvp.lights[idx].color_and_radius[3] = light->radius;

                    lights_found++;
                }
            }
        }
    }

    // Bucle de renderizado por sub-malla
    for (Uint32 i = 0; i < model->mesh_count; i++) {
        KowaiMesh* mesh = &model->meshes[i];

        SDL_GPUBufferBinding vb_binding = { .buffer = mesh->vertex_buffer, .offset = 0 };
        SDL_BindGPUVertexBuffers(renderer->current_render_pass, 0, &vb_binding, 1);

        SDL_GPUBufferBinding ib_binding = { .buffer = mesh->index_buffer, .offset = 0 };
        SDL_BindGPUIndexBuffer(renderer->current_render_pass, &ib_binding, SDL_GPU_INDEXELEMENTSIZE_32BIT);

        if (mesh->texture && mesh->sampler) {
            SDL_GPUTextureSamplerBinding tex_binding = { .texture = mesh->texture, .sampler = mesh->sampler };
            SDL_BindGPUFragmentSamplers(renderer->current_render_pass, 0, &tex_binding, 1);
        }
        else {
            SDL_GPUTextureSamplerBinding fallback_binding = { .texture = renderer->default_texture, .sampler = renderer->default_sampler };
            SDL_BindGPUFragmentSamplers(renderer->current_render_pass, 0, &fallback_binding, 1);
        }

        // Empujar los datos unificados del UBO (Slot 0 en Vertex Shader)
        SDL_PushGPUVertexUniformData(renderer->current_cmd_buffer, 0, &mvp, sizeof(KowaiMVP));

        SDL_DrawGPUIndexedPrimitives(renderer->current_render_pass, mesh->index_count, 1, 0, 0, 0);
    }
}

static SDL_GPUShader* kowai_load_shader(
    SDL_GPUDevice* gpu_device,
    const char* filepath,
    SDL_GPUShaderStage stage,
    Uint32 num_samplers,
    Uint32 num_storage_textures,
    Uint32 num_storage_buffers,
    Uint32 num_uniform_buffers)
{
    size_t code_size = 0;
    Uint8* code_bytes = kowai_read_binary_file(filepath, &code_size);
    if (!code_bytes) return NULL;

    SDL_GPUShaderCreateInfo create_info = {
        .code_size = code_size,
        .code = code_bytes,
        .entrypoint = "main",
        .format = SDL_GPU_SHADERFORMAT_SPIRV,
        .stage = stage,
        .num_samplers = num_samplers,
        .num_storage_textures = num_storage_textures,
        .num_storage_buffers = num_storage_buffers,
        .num_uniform_buffers = num_uniform_buffers
    };

    SDL_GPUShader* shader = SDL_CreateGPUShader(gpu_device, &create_info);
    free(code_bytes);

    if (!shader) {
        SDL_Log("Renderer Error: No se pudo crear el objeto shader desde %s: %s", filepath, SDL_GetError());
    }

    SDL_Log("Shader %s -> %p | samplers=%u uniform_buffers=%u", filepath, (void*)shader, num_samplers, num_uniform_buffers);
    return shader;
}

void kowai_renderer_set_environment(KowaiRenderer* renderer, float* clear, float* sun_dir, float* sun_col, float* ambient) {
    if (!renderer) return;
    if (clear)   glm_vec3_copy(clear, renderer->current_clear_color);
    if (sun_dir) glm_vec3_copy(sun_dir, renderer->current_sun_dir);
    if (sun_col) glm_vec3_copy(sun_col, renderer->current_sun_color);
    if (ambient) glm_vec3_copy(ambient, renderer->current_ambient_color);
}

SDL_GPUDevice* kowai_renderer_get_device(KowaiRenderer* renderer) {
    return renderer ? renderer->gpu_device : NULL;
}

SDL_GPUCommandBuffer* kowai_renderer_get_current_cmd_buffer(KowaiRenderer* renderer) {
    return renderer ? renderer->current_cmd_buffer : NULL;
}

SDL_GPURenderPass* kowai_renderer_get_current_render_pass(KowaiRenderer* renderer) {
    return renderer ? renderer->current_render_pass : NULL;
}

void kowai_renderer_destroy(KowaiRenderer* renderer) {
    if (!renderer) return;

    // Eliminamos imgui_backend_shutdown() de aquí, ya que engine.c se encarga ahora.

    if (renderer->default_texture) SDL_ReleaseGPUTexture(renderer->gpu_device, renderer->default_texture);
    if (renderer->default_sampler) SDL_ReleaseGPUSampler(renderer->gpu_device, renderer->default_sampler);
    if (renderer->depth_texture) SDL_ReleaseGPUTexture(renderer->gpu_device, renderer->depth_texture);
    if (renderer->pipeline) SDL_ReleaseGPUGraphicsPipeline(renderer->gpu_device, renderer->pipeline);
    if (renderer->vertex_shader) SDL_ReleaseGPUShader(renderer->gpu_device, renderer->vertex_shader);
    if (renderer->fragment_shader) SDL_ReleaseGPUShader(renderer->gpu_device, renderer->fragment_shader);

    SDL_DestroyGPUDevice(renderer->gpu_device);
    free(renderer);
    SDL_Log("KowaiRenderer: Destruido correctamente.");
}