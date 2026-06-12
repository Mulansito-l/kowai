#ifndef KOWAI_TIME_H
#define KOWAI_TIME_H

#include <SDL3/SDL.h>

typedef struct {
    uint64_t start_time;
    uint64_t last_time;
    uint64_t current_time;

    double delta_time;       // Tiempo del frame actual en segundos (para ImGui/Shaders)
    double accumulator;      // Guarda el tiempo restante para los ticks fijos
    double fixed_delta_time; // Cuánto tiempo debe durar un tick (ej. 1.0 / 60.0)

    float fps;
    int frame_count;
    uint64_t fps_timer;
} KowaiTimer;

KowaiTimer* kowai_timer_create(double target_fps);
void kowai_timer_destroy(KowaiTimer* timer);
void kowai_timer_update(KowaiTimer* timer);

#endif