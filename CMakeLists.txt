cmake_minimum_required(VERSION 3.15)
project(fishtank CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

set(FISHTANK_CORE_SOURCES
    src/boids.cpp
    src/fish_mesh.cpp
    src/renderer.cpp
    src/shader.cpp
    src/app.cpp
)

if(EMSCRIPTEN)
    # --- Web target: builds fishtank.js + fishtank.wasm ---
    add_executable(fishtank ${FISHTANK_CORE_SOURCES} src/main_web.cpp)
    target_include_directories(fishtank PRIVATE include)

    target_link_options(fishtank PRIVATE
        "-sUSE_WEBGL2=1"
        "-sMIN_WEBGL_VERSION=2"
        "-sMAX_WEBGL_VERSION=2"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sMODULARIZE=1"
        "-sEXPORT_NAME=FishtankModule"
        "-sEXPORTED_FUNCTIONS=[_ft_init,_ft_frame,_ft_resize,_ft_feed_at,_ft_set_fish_count,_malloc,_free]"
        "-sEXPORTED_RUNTIME_METHODS=[ccall,cwrap]"
        "-sENVIRONMENT=web"
        "-sNO_EXIT_RUNTIME=1"
    )
    target_compile_options(fishtank PRIVATE -O3)
    target_link_options(fishtank PRIVATE -O3)

else()
    # --- Native target: a GLFW window for fast local iteration on the sim/renderer.
    # GL functions beyond 1.1 are loaded at runtime by src/gl_loader.cpp — no
    # GLEW/glad dependency needed. ---
    find_package(OpenGL REQUIRED)
    find_package(glfw3 REQUIRED)

    add_executable(fishtank_native ${FISHTANK_CORE_SOURCES} src/gl_loader.cpp src/main_native.cpp)
    target_include_directories(fishtank_native PRIVATE include)
    target_link_libraries(fishtank_native PRIVATE glfw OpenGL::GL)
endif()
