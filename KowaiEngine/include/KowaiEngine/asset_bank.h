#ifndef KOWAI_ASSET_BANK_H
#define KOWAI_ASSET_BANK_H

#include "KowaiEngine/model.h"
// #include "KowaiEngine/texture.h" // ??? Agrega tus cabeceras cuando las crees
// #include "KowaiEngine/audio.h"   // ?? 

#include <SDL3/SDL.h>

#define MAX_ASSETS 128 // ?? Subimos el límite ya que ahora manejaremos texturas/audios

typedef uint32_t KowaiAssetID;

// ?? 1. Enumeración para identificar qué hay dentro del slot
typedef enum {
    ASSET_TYPE_NONE = 0,
    ASSET_TYPE_MODEL,
    ASSET_TYPE_TEXTURE,
    ASSET_TYPE_AUDIO
} KowaiAssetType;

// ?? 2. La Unión: El slot puede ser CUALQUIERA de estos, compartiendo la misma memoria
typedef union {
    KowaiModel* model;
    void* texture; // Cambiar por KowaiTexture* cuando lo tengas
    void* audio;   // Cambiar por KowaiAudio* cuando lo tengas
    void* raw_ptr; // Puntero genérico para limpiezas rápidas
} KowaiAssetPtr;

typedef struct {
    KowaiAssetID hash;
    char id[64];           // "cat_statue", "michi_albedo", "meow_sfx"
    KowaiAssetType type;   // ?? Tipo de asset guardado en este slot
    KowaiAssetPtr  asset;  // ?? Puntero polimórfico union
} KowaiAssetSlot;

typedef struct KowaiAssetBank {
    KowaiAssetSlot slots[MAX_ASSETS];
    int asset_count;
} KowaiAssetBank;

// lifecycle
void kowai_asset_bank_init(KowaiAssetBank* bank);
void kowai_asset_bank_clear(SDL_GPUDevice* device, KowaiAssetBank* bank);

// API - Modelos (Tus funciones existentes)
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

// ?? 3. API Expandida - Texturas y Audios
void* kowai_asset_bank_register_texture(
    SDL_GPUDevice* device,
    KowaiAssetBank* bank,
    const char* id,
    const char* file_path
);

void* kowai_asset_bank_get_texture(
    KowaiAssetBank* bank,
    const char* id
);

// reverse lookup (para scene save)
const char* kowai_asset_bank_get_id_from_model(
    KowaiAssetBank* bank,
    KowaiModel* model
);

#endif