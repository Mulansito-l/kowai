#include "KowaiEngine/asset_bank.h"
#include <string.h>
#include <stdlib.h>

SDL_GPUTexture* kowai_internal_load_texture(SDL_GPUDevice* gpu_device, const char* filepath);

// -----------------------------------------------------
// HASH SIMPLE (rápido y suficiente para engine)
// -----------------------------------------------------
static KowaiAssetID kowai_hash_id(const char* str)
{
    KowaiAssetID hash = 5381;
    int c;

    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;

    return hash;
}

// -----------------------------------------------------
// INIT
// -----------------------------------------------------
void kowai_asset_bank_init(KowaiAssetBank* bank)
{
    if (!bank) return;

    bank->asset_count = 0;

    for (int i = 0; i < MAX_ASSETS; i++) {
        bank->slots[i].id[0] = '\0';
        bank->slots[i].hash = 0;
        bank->slots[i].type = ASSET_TYPE_NONE;
        bank->slots[i].asset.raw_ptr = NULL; // ?? Limpia la unión entera apuntando a NULL
    }
}

// -----------------------------------------------------
// CLEAR VRAM
// -----------------------------------------------------
void kowai_asset_bank_clear(SDL_GPUDevice* device, KowaiAssetBank* bank)
{
    if (!bank) return;

    for (int i = 0; i < bank->asset_count; i++) {
        KowaiAssetSlot* slot = &bank->slots[i];

        if (slot->asset.raw_ptr != NULL) {
            SDL_Log("AssetBank: Freeing asset '%s' [Type: %d]", slot->id, slot->type);

            // ?? Liberamos de acuerdo al tipo de recurso almacenado
            switch (slot->type) {
            case ASSET_TYPE_MODEL:
                if (slot->asset.model) {
                    kowai_model_destroy(device, slot->asset.model);
                }
                break;
            case ASSET_TYPE_TEXTURE:
                // TODO: kowai_texture_destroy(device, slot->asset.texture);
                break;
            case ASSET_TYPE_AUDIO:
                // TODO: kowai_audio_destroy(slot->asset.audio);
                break;
            default:
                break;
            }
            slot->asset.raw_ptr = NULL;
        }
        slot->type = ASSET_TYPE_NONE;
    }

    bank->asset_count = 0;
}

KowaiModel* kowai_asset_bank_register_gltf(
    SDL_GPUDevice* device,
    KowaiAssetBank* bank,
    const char* id,
    const char* file_path)
{
    if (!bank || !id || !file_path) return NULL;

    KowaiAssetID hash = kowai_hash_id(id);

    // 1. Check duplicates (Validando que coincida hash y tipo)
    for (int i = 0; i < bank->asset_count; i++) {
        if (bank->slots[i].hash == hash && bank->slots[i].type == ASSET_TYPE_MODEL) {
            return bank->slots[i].asset.model;
        }
    }

    if (bank->asset_count >= MAX_ASSETS) {
        SDL_Log("AssetBank: FULL");
        return NULL;
    }

    // =====================================================================
    // 🟢 NUEVO: DETECCIÓN EN CALIENTE DE EXTENSIÓN (.gltf vs .glb)
    // =====================================================================
    KowaiModel* model = NULL;
    const char* ext = SDL_strrchr(file_path, '.');

    if (ext && SDL_strcasecmp(ext, ".glb") == 0) {
        // Carga binaria optimizada ignorando materiales/texturas internas
        model = kowai_model_load_geometry_only(device, file_path);
    }
    else {
        // Tu carga tradicional por defecto para archivos de texto .gltf
        model = kowai_model_load_gltf(device, file_path);
    }
    // =====================================================================

    if (!model) {
        SDL_Log("AssetBank: failed loading %s", file_path);
        return NULL;
    }

    // 2. Asignación al slot del Asset Bank
    int slot_idx = bank->asset_count++;

    bank->slots[slot_idx].hash = hash;
    SDL_strlcpy(bank->slots[slot_idx].id, id, sizeof(bank->slots[slot_idx].id));
    SDL_strlcpy(bank->slots[slot_idx].path, file_path, sizeof(bank->slots[slot_idx].path)); // Guardamos path para el Scene Save
    bank->slots[slot_idx].type = ASSET_TYPE_MODEL;
    bank->slots[slot_idx].asset.model = model;

    SDL_Log("AssetBank: Registrado modelo '%s' desde [%s]", id, file_path);

    return model;
}

// -----------------------------------------------------
// GET MODEL BY ID
// -----------------------------------------------------
KowaiModel* kowai_asset_bank_get_model(
    KowaiAssetBank* bank,
    const char* id)
{
    if (!bank || !id) return NULL;

    KowaiAssetID hash = kowai_hash_id(id);

    for (int i = 0; i < bank->asset_count; i++) {
        if (bank->slots[i].hash == hash && bank->slots[i].type == ASSET_TYPE_MODEL) {
            return bank->slots[i].asset.model; // ?? Retorna desde la unión
        }
    }

    return NULL;
}

// -----------------------------------------------------
// REGISTER TEXTURE (Implementación Real con SDL3 GPU)
// -----------------------------------------------------
void* kowai_asset_bank_register_texture(
    SDL_GPUDevice* device,
    KowaiAssetBank* bank,
    const char* id,
    const char* file_path)
{
    if (!device || !bank || !id || !file_path) return NULL;

    // 1. Generar el hash único del identificador de la textura
    KowaiAssetID hash = kowai_hash_id(id);

    // 2. Verificar si el recurso ya existe en el banco para no duplicar en VRAM
    for (int i = 0; i < bank->asset_count; i++) {
        if (bank->slots[i].hash == hash && bank->slots[i].type == ASSET_TYPE_TEXTURE) {
            SDL_Log("AssetBank: La textura '%s' ya se encuentra cargada en memoria.", id);
            return bank->slots[i].asset.texture;
        }
    }

    // 3. Control de límites del pool plano estático
    if (bank->asset_count >= MAX_ASSETS) {
        SDL_Log("AssetBank Error: Capacidad máxima de recursos alcanzada (%d). No se puede registrar '%s'.", MAX_ASSETS, id);
        return NULL;
    }

    // 4. 🎨 Cargar el archivo de imagen físico y subir los píxeles a la GPU
    // Usamos directamente tu función auxiliar estática que maneja stbi_load y SDL_GPUCopyPass
    SDL_GPUTexture* native_texture = kowai_internal_load_texture(device, file_path);

    if (!native_texture) {
        SDL_Log("AssetBank Error: No se pudieron cargar los píxeles o inicializar el recurso de textura en: %s", file_path);
        return NULL;
    }

    // 5. Reservar el slot disponible e inyectar los metadatos y el puntero nativo de la GPU
    int slot_idx = bank->asset_count++;
    bank->slots[slot_idx].hash = hash;
    SDL_strlcpy(bank->slots[slot_idx].id, id, sizeof(bank->slots[slot_idx].id));
    bank->slots[slot_idx].type = ASSET_TYPE_TEXTURE;

    // Mapeamos el puntero SDL_GPUTexture* al campo .texture (void*) de la unión de tu asset slot
    bank->slots[slot_idx].asset.texture = (void*)native_texture;

    SDL_Log("VRAM: Textura '%s' registrada exitosamente desde disco físico en el slot [%d].", id, slot_idx);

    SDL_strlcpy(bank->slots[slot_idx].path, file_path, sizeof(bank->slots[slot_idx].path));

    return (void*)native_texture;
}

// -----------------------------------------------------
// GET TEXTURE BY ID
// -----------------------------------------------------
void* kowai_asset_bank_get_texture(KowaiAssetBank* bank, const char* id)
{
    if (!bank || !id) return NULL;

    KowaiAssetID hash = kowai_hash_id(id);

    for (int i = 0; i < bank->asset_count; i++) {
        if (bank->slots[i].hash == hash && bank->slots[i].type == ASSET_TYPE_TEXTURE) {
            return bank->slots[i].asset.texture;
        }
    }

    return NULL;
}

// -----------------------------------------------------
// REVERSE LOOKUP (MODEL -> ID)
// -----------------------------------------------------
const char* kowai_asset_bank_get_id_from_model(
    KowaiAssetBank* bank,
    KowaiModel* model)
{
    if (!bank || !model) return "unknown";

    for (int i = 0; i < bank->asset_count; i++) {
        // ?? Aseguramos que solo busque en slots que sean modelos
        if (bank->slots[i].type == ASSET_TYPE_MODEL && bank->slots[i].asset.model == model) {
            return bank->slots[i].id;
        }
    }

    return "unknown";
}