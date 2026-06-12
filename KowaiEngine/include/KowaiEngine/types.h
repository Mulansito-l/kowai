#ifndef KOWAI_TYPES_H
#define KOWAI_TYPES_H

// 
// TIPOS PARA GRAFICOS
// 
// Estructura de Vértice alineada para la GPU
typedef struct {
    float x, y, z;    // Posición en el espacio 3D (Offset: 0 bytes)
    float nx, ny, nz; // Normales para iluminación (Offset: 12 bytes)
    float u, v;       // Coordenadas UV para texturas (Offset: 24 bytes)
} KowaiVertex;        // Tamaño total = 32 bytes (8 floats * 4 bytes)

#endif // KOWAI_TYPES_H