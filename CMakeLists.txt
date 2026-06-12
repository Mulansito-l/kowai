cmake_minimum_required(VERSION 3.24) # Se recomienda 3.24+ para un mejor manejo de FetchContent
project(Kowai LANGUAGES C CXX)

# Configuración del estándar C11/C17
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Organizar los proyectos en carpetas dentro de Visual Studio
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# ---------------------------------------------------------------------------
# DESCARGA AUTOMÁTICA DE SDL3 DESDE GITHUB
# ---------------------------------------------------------------------------
include(FetchContent)

# Desactivamos opciones de SDL3 que no necesitamos para compilar más rápido
set(SDL_SHARED ON CACHE BOOL "Build shared SDL library" FORCE)
set(SDL_STATIC OFF CACHE BOOL "Build static SDL library" FORCE)
set(SDL_TEST OFF CACHE BOOL "Build SDL test applications" FORCE)

message(STATUS "Descargando SDL3 desde GitHub...")

FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        main # Puedes cambiar "main" por un hash de commit específico para mayor estabilidad
)

FetchContent_MakeAvailable(SDL3)

# ---------------------------------------------------------------------------
# INCLUIR SUBPROYECTOS
# ---------------------------------------------------------------------------
add_subdirectory(KowaiEngine)
add_subdirectory(KowaiTest)