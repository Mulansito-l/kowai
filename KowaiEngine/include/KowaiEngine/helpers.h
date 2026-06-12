#include <stdio.h>
#include <stdlib.h>
#include <SDL3/SDL.h>

// --- FUNCIÓN AUXILIAR: Lee un archivo binario completo desde el disco ---
static Uint8* kowai_read_binary_file(const char* filepath, size_t* out_size) {
    FILE* file = fopen(filepath, "rb");
    if (!file) {
        SDL_Log("Renderer Error: No se pudo abrir el archivo shader: %s", filepath);
        return NULL;
    }

    // Mover el cursor al final para calcular el tamaño en bytes
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (size <= 0) {
        SDL_Log("Renderer Error: El archivo %s está vacío.", filepath);
        fclose(file);
        return NULL;
    }

    // Reservar memoria para los bytes
    Uint8* buffer = (Uint8*)malloc(size);
    if (!buffer) {
        SDL_Log("Renderer Error: Fallo de asignación de memoria para %s", filepath);
        fclose(file);
        return NULL;
    }

    // Leer los datos del disco al búfer
    size_t read_bytes = fread(buffer, 1, size, file);
    fclose(file);

    if (read_bytes != size) {
        SDL_Log("Renderer Error: Error al leer los bytes de %s", filepath);
        free(buffer);
        return NULL;
    }

    *out_size = (size_t)size;
    return buffer;
}

static bool kowai_read_line(SDL_IOStream* file, char* out, size_t max)
{
    if (!file || !out || max == 0) return false;

    size_t i = 0;
    char c;
    size_t read;

    while (i < max - 1)
    {
        read = SDL_ReadIO(file, &c, 1);
        if (read <= 0) break;

        if (c == '\n')
            break;

        out[i++] = c;
    }

    if (i == 0 && read <= 0)
        return false;

    out[i] = '\0';
    return true;
}