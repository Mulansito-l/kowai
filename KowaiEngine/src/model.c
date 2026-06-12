#define CGLTF_IMPLEMENTATION
#include "KowaiEngine/cgltf.h"

#define STB_IMAGE_IMPLEMENTATION
#include "KowaiEngine/stb_image.h"

#include "KowaiEngine/model.h"
#include "KowaiEngine/types.h"
#include <stdio.h>
#include <stdlib.h>

// --- AUXILIAR: Cargar e Inyectar textura en la VRAM ---
static SDL_GPUTexture* kowai_internal_load_texture(SDL_GPUDevice* gpu_device, const char* filepath) {
    int w, h, comp;
    stbi_uc* pixels = stbi_load(filepath, &w, &h, &comp, STBI_rgb_alpha);
    if (!pixels) return NULL;

    SDL_GPUTextureCreateInfo t_info = {
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        .width = (Uint32)w, .height = (Uint32)h, .layer_count_or_depth = 1,
        .num_levels = 1, .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER
    };
    SDL_GPUTexture* texture = SDL_CreateGPUTexture(gpu_device, &t_info);

    Uint32 size = w * h * 4;
    SDL_GPUTransferBufferCreateInfo tb_info = { .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = size };
    SDL_GPUTransferBuffer* tb = SDL_CreateGPUTransferBuffer(gpu_device, &tb_info);

    void* data = SDL_MapGPUTransferBuffer(gpu_device, tb, false);
    SDL_memcpy(data, pixels, size);
    SDL_UnmapGPUTransferBuffer(gpu_device, tb);

    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(gpu_device);
    SDL_GPUCopyPass* cp = SDL_BeginGPUCopyPass(cmd);
    SDL_GPUTextureTransferInfo src = { .transfer_buffer = tb, .offset = 0 };
    SDL_GPUTextureRegion dst = { .texture = texture, .w = (Uint32)w, .h = (Uint32)h, .d = 1 };

    SDL_UploadToGPUTexture(cp, &src, &dst, false);
    SDL_EndGPUCopyPass(cp);
    SDL_SubmitGPUCommandBuffer(cmd);

    SDL_ReleaseGPUTransferBuffer(gpu_device, tb);
    stbi_image_free(pixels);
    return texture;
}

// --- AUXILIAR: Crear un Buffer en la GPU (Vertex / Index) y subirle datos ---
static SDL_GPUBuffer* kowai_internal_create_gpu_buffer(SDL_GPUDevice* gpu_device, SDL_GPUBufferUsageFlags usage, const void* data, Uint32 size) {
    SDL_GPUBufferCreateInfo b_info = { .usage = usage, .size = size };
    SDL_GPUBuffer* gpu_buffer = SDL_CreateGPUBuffer(gpu_device, &b_info);

    SDL_GPUTransferBufferCreateInfo tb_info = { .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = size };
    SDL_GPUTransferBuffer* tb = SDL_CreateGPUTransferBuffer(gpu_device, &tb_info);

    void* mapped = SDL_MapGPUTransferBuffer(gpu_device, tb, false);
    SDL_memcpy(mapped, data, size);
    SDL_UnmapGPUTransferBuffer(gpu_device, tb);

    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(gpu_device);
    SDL_GPUCopyPass* cp = SDL_BeginGPUCopyPass(cmd);
    SDL_GPUTransferBufferLocation src = { .transfer_buffer = tb, .offset = 0 };
    SDL_GPUBufferRegion dst = { .buffer = gpu_buffer, .offset = 0, .size = size };

    SDL_UploadToGPUBuffer(cp, &src, &dst, false);
    SDL_EndGPUCopyPass(cp);
    SDL_SubmitGPUCommandBuffer(cmd);

    SDL_ReleaseGPUTransferBuffer(gpu_device, tb);
    return gpu_buffer;
}

// --- FUNCIÓN PRINCIPAL DEL CARGADOR ---
KowaiModel* kowai_model_load_gltf(SDL_GPUDevice* gpu_device, const char* filepath) {
    cgltf_options options = { 0 };
    cgltf_data* data = NULL;
    cgltf_result result = cgltf_parse_file(&options, filepath, &data);

    if (result != cgltf_result_success) {
        SDL_Log("Model Error: No se pudo parsear el archivo glTF: %s", filepath);
        return NULL;
    }
    cgltf_load_buffers(&options, data, filepath);

    KowaiModel* model = malloc(sizeof(KowaiModel));
    model->mesh_count = (Uint32)data->meshes_count;
    model->meshes = malloc(sizeof(KowaiMesh) * model->mesh_count);

    // Iterar por cada malla definida dentro del archivo glTF
    for (Uint32 m = 0; m < model->mesh_count; m++) {
        cgltf_primitive* prim = &data->meshes[m].primitives[0];
        KowaiMesh* current_mesh = &model->meshes[m];

        size_t vertex_count = prim->attributes[0].data->count;
        KowaiVertex* vertices = calloc(vertex_count, sizeof(KowaiVertex));

        // Mapear los atributos a nuestro layout intercalado (Interleaved Layout)
        for (size_t a = 0; a < prim->attributes_count; a++) {
            cgltf_attribute* attr = &prim->attributes[a];
            if (attr->type == cgltf_attribute_type_position) {
                for (size_t v = 0; v < vertex_count; v++)
                    cgltf_accessor_read_float(attr->data, v, &vertices[v].x, 3);
            }
            else if (attr->type == cgltf_attribute_type_normal) {
                for (size_t v = 0; v < vertex_count; v++)
                    cgltf_accessor_read_float(attr->data, v, &vertices[v].nx, 3);
            }
            else if (attr->type == cgltf_attribute_type_texcoord) {
                for (size_t v = 0; v < vertex_count; v++)
                    cgltf_accessor_read_float(attr->data, v, &vertices[v].u, 2);
            }
        }

        // Leer los Índices
        current_mesh->index_count = (Uint32)prim->indices->count;
        Uint32* indices = malloc(sizeof(Uint32) * current_mesh->index_count);
        for (size_t i = 0; i < current_mesh->index_count; i++) {
            indices[i] = (Uint32)cgltf_accessor_read_index(prim->indices, i);
        }

        // Subir buffers geométricos a la GPU de forma atómica
        current_mesh->vertex_buffer = kowai_internal_create_gpu_buffer(gpu_device, SDL_GPU_BUFFERUSAGE_VERTEX, vertices, sizeof(KowaiVertex) * vertex_count);
        current_mesh->index_buffer = kowai_internal_create_gpu_buffer(gpu_device, SDL_GPU_BUFFERUSAGE_INDEX, indices, sizeof(Uint32) * current_mesh->index_count);

        // --- CARGAR TEXTURAS Y SAMPLERS DE LA SUB-MALLA ---
        if (data->textures_count > 0 && prim->material && prim->material->has_pbr_metallic_roughness) {
            cgltf_texture* tex = prim->material->pbr_metallic_roughness.base_color_texture.texture;
            if (tex && tex->image && tex->image->uri) {

                char full_texture_path[512];
                SDL_zeroa(full_texture_path);

                // Buscamos dónde termina la carpeta del archivo glTF
                const char* last_slash = SDL_strrchr(filepath, '/');
                if (!last_slash) {
                    last_slash = SDL_strrchr(filepath, '\\'); // Compatibilidad con Windows
                }

                if (last_slash) {
                    // Calculamos el tamaño exacto del directorio EXCLUYENDO el nombre del .gltf
                    // last_slash - filepath nos da los caracteres hasta la barra inclusive
                    size_t dir_len = (size_t)(last_slash - filepath) + 1;

                    SDL_strlcpy(full_texture_path, filepath, dir_len + 1);
                    SDL_strlcat(full_texture_path, tex->image->uri, sizeof(full_texture_path));
                }
                else {
                    // Si el .gltf está suelto en la raíz de ejecución
                    SDL_strlcpy(full_texture_path, tex->image->uri, sizeof(full_texture_path));
                }

                SDL_Log("KowaiEngine: Intentando cargar textura desde: %s", full_texture_path);
                current_mesh->texture = kowai_internal_load_texture(gpu_device, full_texture_path);
            }
        }

        // Si el modelo no tenía textura, creamos un Sampler por defecto para evitar crashes
        SDL_GPUSamplerCreateInfo samplerInfo = {
            .min_filter = SDL_GPU_FILTER_NEAREST, // 🔴 Pixelado al alejarse
            .mag_filter = SDL_GPU_FILTER_NEAREST, // 🔴 Pixelado al acercarse
            .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
            .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
            .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
            .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT
        };
        current_mesh->sampler = SDL_CreateGPUSampler(gpu_device, &samplerInfo);

        free(vertices);
        free(indices);
    }

    cgltf_free(data);
    SDL_Log("KowaiEngine: Modelo glTF cargado correctamente con %d sub-mallas.", model->mesh_count);
    return model;
}

void kowai_model_destroy(SDL_GPUDevice* gpu_device, KowaiModel* model) {
    if (!model) return;
    for (Uint32 i = 0; i < model->mesh_count; i++) {
        SDL_ReleaseGPUBuffer(gpu_device, model->meshes[i].vertex_buffer);
        SDL_ReleaseGPUBuffer(gpu_device, model->meshes[i].index_buffer);
        if (model->meshes[i].texture) SDL_ReleaseGPUTexture(gpu_device, model->meshes[i].texture);
        if (model->meshes[i].sampler) SDL_ReleaseGPUSampler(gpu_device, model->meshes[i].sampler);
    }
    free(model->meshes);
    free(model);
}