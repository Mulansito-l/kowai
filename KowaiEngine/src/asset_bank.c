#include "KowaiEngine/asset_bank.h"
#include <string.h>
#include <stdlib.h>

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

// -----------------------------------------------------
// REGISTER GLTF (Modelos)
// -----------------------------------------------------
KowaiModel* kowai_asset_bank_register_gltf(
    SDL_GPUDevice* device,
    KowaiAssetBank* bank,
    const char* id,
    const char* file_path)
{
    if (!bank || !id || !file_path) return NULL;

    KowaiAssetID hash = kowai_hash_id(id);

    // Check duplicates (Validando que coincida hash y tipo)
    for (int i = 0; i < bank->asset_count; i++) {
        if (bank->slots[i].hash == hash && bank->slots[i].type == ASSET_TYPE_MODEL) {
            return bank->slots[i].asset.model;
        }
    }

    if (bank->asset_count >= MAX_ASSETS) {
        SDL_Log("AssetBank: FULL");
        return NULL;
    }

    KowaiModel* model = kowai_model_load_gltf(device, file_path);
    if (!model) {
        SDL_Log("AssetBank: failed loading %s", file_path);
        return NULL;
    }

    int slot_idx = bank->asset_count++;

    bank->slots[slot_idx].hash = hash;
    SDL_strlcpy(bank->slots[slot_idx].id, id, sizeof(bank->slots[slot_idx].id));
    bank->slots[slot_idx].type = ASSET_TYPE_MODEL;       // ?? Seteamos tipo
    bank->slots[slot_idx].asset.model = model;           // ?? Guardamos en la unión

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
// REGISTER TEXTURE (Plantilla lista para usar)
// -----------------------------------------------------
void* kowai_asset_bank_register_texture(
    SDL_GPUDevice* device,
    KowaiAssetBank* bank,
    const char* id,
    const char* file_path)
{
    if (!bank || !id || !file_path) return NULL;

    KowaiAssetID hash = kowai_hash_id(id);

    for (int i = 0; i < bank->asset_count; i++) {
        if (bank->slots[i].hash == hash && bank->slots[i].type == ASSET_TYPE_TEXTURE) {
            return bank->slots[i].asset.texture;
        }
    }

    if (bank->asset_count >= MAX_ASSETS) {
        SDL_Log("AssetBank: FULL");
        return NULL;
    }

    // Aquí llamarías a tu cargador de texturas de SDL_GPU, ej:
    // KowaiTexture* tex = kowai_texture_load(device, file_path);
    void* tex = (void*)0xDEADBEEF; // Mock temporal para que compile y pruebes

    if (!tex) return NULL;

    int slot_idx = bank->asset_count++;
    bank->slots[slot_idx].hash = hash;
    SDL_strlcpy(bank->slots[slot_idx].id, id, sizeof(bank->slots[slot_idx].id));
    bank->slots[slot_idx].type = ASSET_TYPE_TEXTURE;
    bank->slots[slot_idx].asset.texture = tex;

    return tex;
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