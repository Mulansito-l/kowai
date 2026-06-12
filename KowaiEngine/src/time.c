#include "KowaiEngine/time.h"

KowaiTimer* kowai_timer_create(double target_fps) {
    KowaiTimer* timer = (KowaiTimer*)malloc(sizeof(KowaiTimer));
    if (!timer) return NULL;

    timer->start_time = SDL_GetTicksNS();
    timer->last_time = timer->start_time;
    timer->current_time = timer->start_time;

    timer->delta_time = 0.0;
    timer->accumulator = 0.0;
    timer->fixed_delta_time = 1.0 / target_fps; // 60 FPS = 0.016666s por tick

    timer->fps = 0.0f;
    timer->frame_count = 0;
    timer->fps_timer = timer->start_time;

    return timer;
}

void kowai_timer_destroy(KowaiTimer* timer) {
    if (timer) free(timer);
}

void kowai_timer_update(KowaiTimer* timer) {
    timer->current_time = SDL_GetTicksNS();

    // Calcular tiempo transcurrido en nanosegundos y pasarlo a segundos
    uint64_t elapsed_ns = timer->current_time - timer->last_time;
    timer->last_time = timer->current_time;

    // Evitar el "clamping" o congelamientos si la ventana se arrastra
    double frame_time = (double)elapsed_ns / 1000000000.0;
    if (frame_time > 0.25) frame_time = 0.25;

    timer->delta_time = frame_time;
    timer->accumulator += frame_time;

    // --- CÓMPUTO DE FPS REALES EN CONSOLA/MOTOR ---
    timer->frame_count++;
    if (timer->current_time - timer->fps_timer >= 1000000000) { // Pasó 1 segundo
        timer->fps = (float)timer->frame_count;
        timer->frame_count = 0;
        timer->fps_timer = timer->current_time;
    }
}