#include "KowaiEngine/camera.h"
#include <math.h>

void kowai_camera_init(KowaiCamera* camera, KowaiCameraMode mode) {
    // Posición inicial por defecto un poco hacia atrás (Z = 3.0f)
    glm_vec3_copy((vec3) { 0.0f, 0.0f, 3.0f }, camera->position);
    glm_vec3_copy((vec3) { 0.0f, 1.0f, 0.0f }, camera->world_up);

    camera->yaw = -90.0f; // Mirando hacia el fondo de la pantalla (Z negativo)
    camera->pitch = 0.0f;
    camera->fov = 45.0f;
    camera->speed = 4.0f;        // 4 unidades por segundo
    camera->sensitivity = 0.1f;  // Sensibilidad del ratón
    camera->mode = mode;

    kowai_camera_update_vectors(camera);
}

void kowai_camera_update_vectors(KowaiCamera* camera) {
    // Calcular el nuevo vector Front usando radianes
    vec3 front;
    front[0] = cosf(glm_rad(camera->yaw)) * cosf(glm_rad(camera->pitch));
    front[1] = sinf(glm_rad(camera->pitch));
    front[2] = sinf(glm_rad(camera->yaw)) * cosf(glm_rad(camera->pitch));
    glm_vec3_normalize_to(front, camera->front);

    // Recalcular los vectores Right y Up
    glm_vec3_cross(camera->front, camera->world_up, camera->right);
    glm_vec3_normalize(camera->right);

    glm_vec3_cross(camera->right, camera->front, camera->up);
    glm_vec3_normalize(camera->up);
}

void kowai_camera_get_vectors(const KowaiCamera* camera, vec3 forward, vec3 right) {
    // Calcular vector Forward de manera aislada basándonos exclusivamente en la rotación actual
    float rad_yaw = glm_rad(camera->yaw);
    float rad_pitch = glm_rad(camera->pitch);

    forward[0] = cosf(rad_pitch) * cosf(rad_yaw);
    forward[1] = sinf(rad_pitch);
    forward[2] = cosf(rad_pitch) * sinf(rad_yaw);
    glm_vec3_normalize(forward);

    // Calcular vector Right perpendicular al Forward y al Up global del mundo
    glm_vec3_cross(forward, camera->world_up, right);
    glm_vec3_normalize(right);
}

void kowai_camera_process_input(KowaiCamera* camera, const Uint8* keyboard_state, float mouse_dx, float mouse_dy, float delta_time) {
    // 1. PROCESAR ORIENTACIÓN (MOUSE)
    camera->yaw += mouse_dx * camera->sensitivity;
    camera->pitch -= mouse_dy * camera->sensitivity; // Invertido para comportamiento natural

    // Limitar el cuello del espectador
    if (camera->pitch > 89.0f)  camera->pitch = 89.0f;
    if (camera->pitch < -89.0f) camera->pitch = -89.0f;

    kowai_camera_update_vectors(camera);

    // 2. PROCESAR MOVIMIENTO (TECLADO)
    float velocity = camera->speed * delta_time;
    vec3 move_dir;

    if (keyboard_state[SDL_SCANCODE_W]) { // Adelante
        glm_vec3_scale(camera->front, velocity, move_dir);
        glm_vec3_add(camera->position, move_dir, camera->position);
    }
    if (keyboard_state[SDL_SCANCODE_S]) { // Atrás
        glm_vec3_scale(camera->front, velocity, move_dir);
        glm_vec3_sub(camera->position, move_dir, camera->position);
    }
    if (keyboard_state[SDL_SCANCODE_A]) { // Izquierda (Strafe)
        glm_vec3_scale(camera->right, velocity, move_dir);
        glm_vec3_sub(camera->position, move_dir, camera->position);
    }
    if (keyboard_state[SDL_SCANCODE_D]) { // Derecha (Strafe)
        glm_vec3_scale(camera->right, velocity, move_dir);
        glm_vec3_add(camera->position, move_dir, camera->position);
    }
}

void kowai_camera_process_input_mapped(KowaiCamera* camera, KowaiInputSystem* input, float mouse_dx, float mouse_dy, float dt) {
    // 1. Procesar Rotación (Vuelo)
    camera->yaw += mouse_dx * camera->sensitivity;
    camera->pitch -= mouse_dy * camera->sensitivity; // Invertido para comportamiento estándar

    // Clampear el pitch para evitar dar la vuelta completa vertical
    if (camera->pitch > 89.0f)  camera->pitch = 89.0f;
    if (camera->pitch < -89.0f) camera->pitch = -89.0f;

    // Extraer los vectores de dirección limpios basados en los ángulos actualizados
    vec3 forward, right;
    kowai_camera_get_vectors(camera, forward, right);

    // 2. Procesar Traslación Semántica usando la API nativa sin usar scaleadd
    float speed = camera->speed * dt;
    vec3 displacement;

    if (kowai_input_get_action(input, "MoveForward")) {
        glm_vec3_scale(forward, speed, displacement);
        glm_vec3_add(camera->position, displacement, camera->position);
    }
    if (kowai_input_get_action(input, "MoveBackward")) {
        glm_vec3_scale(forward, speed, displacement);
        glm_vec3_sub(camera->position, displacement, camera->position);
    }
    if (kowai_input_get_action(input, "MoveLeft")) {
        glm_vec3_scale(right, speed, displacement);
        glm_vec3_sub(camera->position, displacement, camera->position);
    }
    if (kowai_input_get_action(input, "MoveRight")) {
        glm_vec3_scale(right, speed, displacement);
        glm_vec3_add(camera->position, displacement, camera->position);
    }

    // Sincronizar los vectores direccionales base de la estructura de la cámara
    kowai_camera_update_vectors(camera);
}

void kowai_camera_update_matrices(KowaiCamera* camera, int width, int height) {
    if (height == 0) height = 1;
    float aspect = (float)width / (float)height;

    // Matriz de Vista: ¿Dónde estoy y hacia dónde veo?
    vec3 target;
    glm_vec3_add(camera->position, camera->front, target);
    glm_lookat(camera->position, target, camera->up, camera->view_matrix);

    // Matriz de Proyección Perspectiva
    glm_perspective(glm_rad(camera->fov), aspect, 0.1f, 100.0f, camera->proj_matrix);
}