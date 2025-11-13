cmake_minimum_required(VERSION 3.28)
project(Laying_Game)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(
        Laying_Game
        main.cpp
        src/Player.cpp
        src/Board.cpp
        src/Game.cpp
        src/Tiles.cpp
        src/RaylibRenderer.cpp
)

if (WIN32)
    target_sources(Laying_Game PRIVATE ressources/icon.rc)
endif ()

target_include_directories(
        Laying_Game PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}
)

set(RAYLIB_ROOT "C:/raylib/raylib" CACHE PATH "Chemin vers les sources raylib")
set(RAYLIB_SRC_DIR "${RAYLIB_ROOT}/src")

if (EXISTS "${RAYLIB_SRC_DIR}/CMakeLists.txt")
    list(PREPEND CMAKE_MODULE_PATH "${RAYLIB_ROOT}/cmake")
    set(PLATFORM Desktop CACHE STRING "Raylib platform" FORCE)
    set(BUILD_EXAMPLES OFF CACHE BOOL "Build raylib examples" FORCE)
    set(BUILD_GAMES OFF CACHE BOOL "Build raylib games" FORCE)
    set(BUILD_TOOLS OFF CACHE BOOL "Build raylib tools" FORCE)
    set(CUSTOMIZE_BUILD OFF CACHE BOOL "Disable feature stripping" FORCE)
    add_subdirectory(${RAYLIB_SRC_DIR} ${CMAKE_CURRENT_BINARY_DIR}/raylib_build)
    target_link_libraries(Laying_Game PRIVATE raylib)
    target_include_directories(Laying_Game PRIVATE ${RAYLIB_SRC_DIR})
else ()
    message(FATAL_ERROR "Impossible de trouver raylib dans ${RAYLIB_ROOT}. Ajustez RAYLIB_ROOT.")
endif ()

if (WIN32)
    # Copie des ressources (textures, icônes...) à côté de l'exécutable.
    set(LG_RESOURCE_DIR "${CMAKE_SOURCE_DIR}/ressources")
    if (EXISTS "${LG_RESOURCE_DIR}")
        add_custom_command(TARGET Laying_Game POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                        "${LG_RESOURCE_DIR}"
                        "$<TARGET_FILE_DIR:Laying_Game>/ressources")
    endif ()

    # Copie des DLL MinGW indispensables pour distribuer l'exécutable.
    get_filename_component(_compiler_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
    set(LG_MINGW_DLLS
            libgcc_s_seh-1.dll
            libstdc++-6.dll
            libwinpthread-1.dll)
    foreach(dll ${LG_MINGW_DLLS})
        set(dll_path "${_compiler_dir}/${dll}")
        if (EXISTS "${dll_path}")
            add_custom_command(TARGET Laying_Game POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                            "${dll_path}"
                            "$<TARGET_FILE_DIR:Laying_Game>/${dll}")
        else ()
            message(WARNING "DLL introuvable: ${dll_path}. Copiez-la manuellement si nécessaire.")
        endif ()
    endforeach()
endif ()

if (WIN32)
    # Masque la console uniquement lors des builds Release.
    set_target_properties(Laying_Game PROPERTIES
            WIN32_EXECUTABLE $<IF:$<CONFIG:Release>,ON,OFF>)
endif ()

