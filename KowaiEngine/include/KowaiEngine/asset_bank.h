#ifndef KOWAI_ASSET_BANK_H
#define KOWAI_ASSET_BANK_H

#include "KowaiEngine/model.h"
#include <SDL3/SDL.h>

#define MAX_ASSETS 64

typedef uint32_t KowaiAssetID;

typedef struct {
    KowaiAssetID hash;
    char id[64];           // "cat_statue"
    KowaiModel* model_ptr; // GPU model
} KowaiAssetSlot;

typedef struct KowaiAssetBank {
    KowaiAssetSlot slots[MAX_ASSETS];
    int asset_count;
} KowaiAssetBank;

// lifecycle
void kowai_asset_bank_init(KowaiAssetBank* bank);
void kowai_asset_bank_clear(SDL_GPUDevice* device, KowaiAssetBank* bank);

// API
KowaiModel* kowai_asset_bank_register_gltf(
    SDL_GPUDevice* device,
    KowaiAssetBank* bank,
    const char* id,
    const char* file_path
);

KowaiModel* kowai_asset_bank_get_model(
    KowaiAssetBank* bank,
    const char* id
);

// reverse lookup (para scene save)
const char* kowai_asset_bank_get_id_from_model(
    KowaiAssetBank* bank,
    KowaiModel* model
);

#endif