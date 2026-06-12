# Archivos fuente de la aplicación de pruebas
set(TEST_SOURCES
    src/main.c
)

# Crear el ejecutable (KowaiTest.exe)
add_executable(KowaiTest ${TEST_SOURCES})

# Enlazar con tu motor (Hereda los headers de tu motor y todo SDL3 automáticamente)
target_link_libraries(KowaiTest PRIVATE KowaiEngine)

# Forzar subsistema de Consola en Visual Studio para evitar el error de invoke_main
if(MSVC)
    set_target_properties(KowaiTest PROPERTIES
        LINK_FLAGS "/SUBSYSTEM:CONSOLE"
    )
endif()

# Configurar la carpeta de ejecución por defecto en VS2022
set_target_properties(KowaiTest PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

# AUTOMACIÓN PARA WINDOWS: Copiar SDL3.dll al directorio del ejecutable (.exe)
add_custom_command(TARGET KowaiTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:SDL3::SDL3-shared>
    $<TARGET_FILE_DIR:KowaiTest>
)