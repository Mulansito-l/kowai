#ifndef KOWAI_INPUT_H
#define KOWAI_INPUT_H

#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_mouse.h>
#include <stdbool.h>

#define KOWAI_MAX_MAPPINGS 32
#define KOWAI_MAX_CONTEXTS 4

typedef enum KowaiInputModifier
{
    INPUT_MOD_NONE = 0,
    INPUT_MOD_CTRL = 1 << 0,
    INPUT_MOD_SHIFT = 1 << 1,
    INPUT_MOD_ALT = 1 << 2
} KowaiInputModifier;

typedef enum {
    INPUT_TYPE_KEYBOARD,
    INPUT_TYPE_MOUSE_BUTTON
} KowaiInputType;

// Estructura que asocia un hardware con una acción abstracta
typedef struct KowaiInputActionMap {
    char action_name[32];
    KowaiInputType type;
    int code;             // Guarda el SDL_Scancode o el índice del botón del mouse
    int modifier;         // Para combinaciones como SDL_KMOD_CTRL, SDL_KMOD_SHIFT, etc.
} KowaiInputActionMap;

// Un contexto contiene un set de mapeos (Ej: Mapa del "Editor", Mapa del "Juego")
typedef struct KowaiInputContext {
    char context_name[32];
    KowaiInputActionMap mappings[KOWAI_MAX_MAPPINGS];
    int mapping_count;
    bool is_active;
    bool is_read_only;
} KowaiInputContext;

// Sistema Global de Input
typedef struct KowaiInputSystem {
    KowaiInputContext contexts[KOWAI_MAX_CONTEXTS];
    int context_count;

    // Almacenamiento del estado del frame actual y anterior (para detectar flancos de subida/bajada)
    const bool* current_keys;
    uint8_t previous_keys[SDL_SCANCODE_COUNT];

    uint32_t current_mouse_state;
    uint32_t previous_mouse_state;
} KowaiInputSystem;

// API Global del Sistema
void kowai_input_init(KowaiInputSystem* sys);
void kowai_input_update(KowaiInputSystem* sys); // Se llama al inicio del frame, después de SDL_PumpEvents

// Gestión de Contextos y Mapeos
int kowai_input_create_context(KowaiInputSystem* sys, const char* name);
void kowai_input_set_context_active(KowaiInputSystem* sys, const char* name, bool active);
void kowai_input_add_mapping(KowaiInputSystem* sys, const char* context_name, const char* action_name, KowaiInputType type, int code, int modifier);
void kowai_input_set_context_read_only(KowaiInputSystem* sys, const char* name, bool read_only);

// Consultas desde el Editor o el Juego
bool kowai_input_get_action(KowaiInputSystem* sys, const char* action_name);        // ¿Se mantiene presionado?
bool kowai_input_get_action_down(KowaiInputSystem* sys, const char* action_name);   // ¿Se presionó ESTE frame? (Just Pressed)
bool kowai_input_get_action_up(KowaiInputSystem* sys, const char* action_name);     // ¿Se soltó ESTE frame?

const char* kowai_input_modifier_to_string(int modifier);
const char* kowai_input_code_to_string(KowaiInputType type, int code);
const char* kowai_input_code_to_string(KowaiInputType type, int code);
const char* kowai_input_type_to_string(KowaiInputType type);

#endif // KOWAI_INPUT_H