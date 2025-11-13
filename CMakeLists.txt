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