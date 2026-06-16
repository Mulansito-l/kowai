# Archivos fuente de la librería del motor
set(ENGINE_SOURCES
    src/engine.c
    "src/renderer.c" 
    "src/model.c" 
    "src/entity.c" 
    "src/imgui_backend.cpp" 
    "src/time.c" 
    "src/camera.c" 
    "src/asset_bank.c"
    "src/scene.c"
    
 "src/input.c" "src/kowai.cpp")

# 🟢 1. AGREGAR FUENTES DE NFD AL MOTOR SEGÚN SISTEMA OPERATIVO
set(NFD_SOURCES
    external/nfd/src/nfd_common.c
)

if(WIN32)
    # Windows usa la API COM nativa (C++)
    list(APPEND NFD_SOURCES external/nfd/src/nfd_win.cpp)
elseif(APPLE)
    # macOS usa Cocoa (Objective-C)
    list(APPEND NFD_SOURCES external/nfd/src/nfd_cocoa.m)
else()
    # Linux (Arch/XFCE) usa Zenity / Portal
    list(APPEND NFD_SOURCES external/nfd/src/nfd_zenity.c)
endif()

# Asegúrate de mapear la ruta donde tienes descargado dear imgui
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui")

# Archivos núcleo de ImGui
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    # Los dos backends oficiales obligatorios para SDL3 GPU
    ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdlgpu3.cpp
)

# Asegúrate de mapear la ruta donde tienes descargado imguizmo
set(IMGUIZMO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/imguizmo/src")

# Archivos de ImGuizmo
set(IMGUIZMO_SOURCES
    ${IMGUIZMO_DIR}/ImGuizmo.cpp
    # Agrega aquí otros archivos si la versión que bajaste incluye ImGradient, ImSequencer, etc.
    # Para el gizmo básico de transformación, con ImGuizmo.cpp suele ser suficiente.
)

# 🟢 2. Crear librería estática (KowaiEngine.lib) INCLUYENDO las fuentes de NFD
add_library(KowaiEngine STATIC 
    ${ENGINE_SOURCES} 
    ${IMGUI_SOURCES} 
    ${NFD_SOURCES} 
    ${IMGUIZMO_SOURCES}
)

# Configurar rutas de inclusión de cabeceras de tu motor
# 🟢 Agregamos la ruta de los headers de NFD (external/nfd/src/include) para que imgui_backend.cpp los vea
target_include_directories(KowaiEngine PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    ${IMGUI_DIR} 
    ${IMGUI_DIR}/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/external/nfd/src/include
    ${IMGUIZMO_DIR} # 🟢 Agregamos la ruta para que compile ImGuizmo.h
)

# Vincular SDL3 de forma pública (Hereda SDL_GPU y todo el ecosistema)
target_link_libraries(KowaiEngine PUBLIC SDL3::SDL3-shared)

# 🟢 3. ENLAZAR LIBRERÍAS DEL SISTEMA OPERATIVO A KOWAIENGINE
# Necesario para que el enlazador no tire errores de símbolos sin resolver (Unresolved Externals) en Windows
if(WIN32)
    target_link_libraries(KowaiEngine PUBLIC ole32 uuid shell32)
endif()

set_property(TARGET KowaiEngine PROPERTY CXX_STANDARD 17)

# Agrupar archivos en el Explorador de Soluciones de VS
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${ENGINE_SOURCES})