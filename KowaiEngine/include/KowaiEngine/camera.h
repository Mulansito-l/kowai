#ifndef KOWAI_CAMERA_H
#define KOWAI_CAMERA_H

#include <KowaiEngine/cglm/cglm.h>
#include <SDL3/SDL.h>
#include "KowaiEngine/input.h"

typedef enum {
    CAMERA_MODE_EDITOR,
    CAMERA_MODE_GAME
} KowaiCameraMode;

// Esta es la definición que engine.c necesita ver COMPLETA
typedef struct KowaiCamera {
    vec3 position;
    vec3 front;
    vec3 up;
    vec3 right;
    vec3 world_up;
    float yaw;
    float pitch;
    float fov;
    float speed;
    float sensitivity;
    mat4 view_matrix;
    mat4 proj_matrix;
    KowaiCameraMode mode;
} KowaiCamera;

void kowai_camera_init(KowaiCamera* camera, KowaiCameraMode mode);
void kowai_camera_update_vectors(KowaiCamera* camera);
void kowai_camera_get_vectors(const KowaiCamera* camera, vec3 forward, vec3 right);
void kowai_camera_process_input(KowaiCamera* camera, const Uint8* keyboard_state, float mouse_dx, float mouse_dy, float delta_time);
void kowai_camera_process_input_mapped(KowaiCamera* camera, KowaiInputSystem* input, float mouse_dx, float mouse_dy, float dt);
void kowai_camera_update_matrices(KowaiCamera* camera, int width, int height);

#endif // KOWAI_CAMERA_H