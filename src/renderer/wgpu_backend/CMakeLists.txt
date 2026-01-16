# Needs this to suppress cmake warning
if(APPLE)
    enable_language(OBJC)
endif()

if(UNIX)
    set(USE_WAYLAND ON)
endif()


# Dawn options
set(DAWN_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(DAWN_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(DAWN_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(DAWN_ENABLE_NULL OFF CACHE BOOL "" FORCE)
set(DAWN_USE_GLFW OFF CACHE BOOL "" FORCE)

# Tint options (Dawn's shader compiler)
set(TINT_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(TINT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(TINT_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(TINT_BUILD_CMD_TOOLS OFF CACHE BOOL "" FORCE)

# Abseil options (Dawn dependency)
set(ABSL_BUILD_TESTING OFF CACHE BOOL "" FORCE)

# Google Test (pulled in by Dawn)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

set(DAWN_FETCH_DEPENDENCIES ON)

set(SKL_WGPU_SOURCES
        renderer_wgpu.cpp 
        render_backend_wgpuimpl.cpp
        utils_wgpu.cpp
        bind_group_wgpu.cpp
        dynamic_shadow_array.cpp
        dynamic_light_converter.cpp
        single_textures_wgpu.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_wgpu.cpp)

set(SKL_WGPU_TARGET_INCLUDE_LIST 
        ${CMAKE_SOURCE_DIR}/include/shared
        ${CMAKE_SOURCE_DIR}/include/renderer/webgpu
        $<BUILD_INTERFACE:${imgui_SOURCE_DIR}>)

if(EMSCRIPTEN)
        add_library(wgpu-backend SHARED ${SKL_WGPU_SOURCES})
        target_include_directories(wgpu-backend PRIVATE ${SKL_WGPU_TARGET_INCLUDE_LIST})

        add_library(webgpu INTERFACE)
        # Fetches emscripten Dawn port
        # Got a lot of help from "https://github.com/eliemichel/WebGPU-distribution/blob/main/emdawnwebgpu/CMakeLists.txt"
        CPMAddPackage(NAME emdawn URL "https://github.com/google/dawn/releases/download/v20250519.170457/emdawnwebgpu_pkg-v20250519.170457.zip")
        set(emdawn_ROOT "${emdawn_SOURCE_DIR}")
        target_link_options(webgpu INTERFACE 
                -sUSE_SDL=3
                -sASYNCIFY
                -sALLOW_MEMORY_GROWTH
                --use-port=${emdawn_ROOT}/emdawnwebgpu.port.py
                --closure-args=--externs=${emdawn_ROOT}/webgpu/src/webgpu-externs.js)
        target_compile_options(webgpu INTERFACE 
                --use-port=${emdawn_ROOT}/emdawnwebgpu.port.py
                -DDAWN_DEBUG_BREAK_ON_ERROR=1
                IMGUI_DISABLE)
        set_property(TARGET webgpu APPEND PROPERTY LINK_DEPENDS 
                ../notes.txt
        )
else()
        add_library(wgpu-backend SHARED ${SKL_WGPU_SOURCES})
        target_include_directories(wgpu-backend PRIVATE ${SKL_WGPU_TARGET_INCLUDE_LIST})

        add_library(webgpu INTERFACE)

        target_compile_definitions(wgpu-backend PUBLIC IMGUI_IMPL_WEBGPU_BACKEND_DAWN)

        # Fetches Dawn
        # set(DAWN_FETCH_DEPENDENCIES ON)
        set(DAWN_BUILD_MONOLITHIC_LIBRARY SHARED)

        CPMAddPackage(NAME dawn URL "https://dawn.googlesource.com/dawn/+archive/refs/heads/chromium/7285.tar.gz")

        target_link_libraries(webgpu INTERFACE dawn::webgpu_dawn)
endif()

# Includes ported SDL3-webgpu conduit
add_subdirectory(${CMAKE_SOURCE_DIR}/include/renderer/webgpu/sdl3webgpu-main sdl3webgpu)

set(SKL_WGPU_PCH_HEADERS
        <iostream>
        <string>
        <vector>
        <algorithm>
        <webgpu/webgpu.h>)

target_precompile_headers(wgpu-backend PRIVATE ${SKL_WGPU_PCH_HEADERS})

target_link_libraries(wgpu-backend PRIVATE SKL_COMPILE_OPTIONS)

target_link_libraries(wgpu-backend PRIVATE glm::glm sdl3webgpu imgui skl-math)