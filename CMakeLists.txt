cmake_minimum_required(VERSION 3.24)

project(SCSR
    VERSION 0.1.0
    DESCRIPTION "Space Combat Simulation Renderer"
    LANGUAGES CXX
)

include(FetchContent)

set(SCSR_SCSE_SOURCE_DIR "" CACHE PATH "Path to an existing SCSE checkout")

set(SCSE_BUILD_CONSOLE OFF CACHE BOOL "Build the SCSE sample console front end" FORCE)

if(SCSR_SCSE_SOURCE_DIR)
    if(NOT EXISTS "${SCSR_SCSE_SOURCE_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "SCSR_SCSE_SOURCE_DIR must point to the root of an SCSE checkout.")
    endif()
    add_subdirectory("${SCSR_SCSE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/scse-engine")
else()
    FetchContent_Declare(scse
        GIT_REPOSITORY https://github.com/Sierrastdio/SCSE.git
        GIT_TAG 113cf64199d8d5d190eaf650d13c6261be44c209
        GIT_SHALLOW FALSE
    )
    FetchContent_MakeAvailable(scse)
endif()

set(SDL_SHARED OFF CACHE BOOL "Build SDL2 as a shared library" FORCE)
set(SDL_STATIC ON CACHE BOOL "Build SDL2 as a static library" FORCE)
set(SDL_TEST OFF CACHE BOOL "Build SDL2 test programs" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "Install SDL2 targets" FORCE)

FetchContent_Declare(SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG 5d249570393f7a37e037abf22cd6012a4cc56a71
    GIT_SHALLOW FALSE
)
FetchContent_MakeAvailable(SDL2)

if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()

FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG f5befd2d29e66809cd1110a152e375a7f1981f06
    GIT_SHALLOW FALSE
)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
endif()

find_package(OpenGL REQUIRED)

if(TARGET SDL2::SDL2-static)
    set(SCSR_SDL2_TARGET SDL2::SDL2-static)
elseif(TARGET SDL2::SDL2)
    set(SCSR_SDL2_TARGET SDL2::SDL2)
else()
    message(FATAL_ERROR "The SDL2 dependency did not provide a usable CMake target.")
endif()

add_library(scsr_imgui
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp"
)
target_include_directories(scsr_imgui PUBLIC "${imgui_SOURCE_DIR}")
target_link_libraries(scsr_imgui PUBLIC ${SCSR_SDL2_TARGET} OpenGL::GL)

add_executable(scsr
    src/main.cpp
    src/app/game_app.cpp
    src/game/game_session.cpp
    src/input/input_controller.cpp
    src/renderer/world_renderer.cpp
    src/ui/game_hud.cpp
)
target_include_directories(scsr PRIVATE src)
target_link_libraries(scsr PRIVATE scse_core ${SCSR_SDL2_TARGET} OpenGL::GL scsr_imgui)
target_compile_features(scsr PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(scsr PRIVATE /W4 /permissive-)
else()
    target_compile_options(scsr PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_custom_command(TARGET scsr POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${SCSE_SOURCE_DIR}/scripts"
            "$<TARGET_FILE_DIR:scsr>/scripts"
)
