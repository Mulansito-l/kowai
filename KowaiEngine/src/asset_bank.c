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
        bank->slots[i].model_ptr = NULL;
        bank->slots[i].hash = 0;
    }
}

// -----------------------------------------------------
// CLEAR VRAM
// -----------------------------------------------------
void kowai_asset_bank_clear(SDL_GPUDevice* device, KowaiAssetBank* bank)
{
    if (!bank) return;

    for (int i = 0; i < bank->asset_count; i++) {
        if (bank->slots[i].model_ptr) {
            SDL_Log("AssetBank: freeing '%s'", bank->slots[i].id);
            kowai_model_destroy(device, bank->slots[i].model_ptr);
            bank->slots[i].model_ptr = NULL;
        }
    }

    bank->asset_count = 0;
}

// -----------------------------------------------------
// REGISTER GLTF
// -----------------------------------------------------
KowaiModel* kowai_asset_bank_register_gltf(
    SDL_GPUDevice* device,
    KowaiAssetBank* bank,
    const char* id,
    const char* file_path)
{
    if (!bank || !id || !file_path) return NULL;

    KowaiAssetID hash = kowai_hash_id(id);

    // check duplicates
    for (int i = 0; i < bank->asset_count; i++) {
        if (bank->slots[i].hash == hash) {
            return bank->slots[i].model_ptr;
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

    int slot = bank->asset_count++;

    bank->slots[slot].hash = hash;
    SDL_strlcpy(bank->slots[slot].id, id, sizeof(bank->slots[slot].id));
    bank->slots[slot].model_ptr = model;

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
        if (bank->slots[i].hash == hash) {
            return bank->slots[i].model_ptr;
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
        if (bank->slots[i].model_ptr == model) {
            return bank->slots[i].id;
        }
    }

    return "unknown";
}