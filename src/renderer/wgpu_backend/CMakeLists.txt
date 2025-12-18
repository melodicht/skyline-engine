# Needs this to suppress cmake warning
if(APPLE)
    enable_language(OBJC)
endif()

if(UNIX)
    set(USE_WAYLAND ON)
endif()

set(DAWN_FETCH_DEPENDENCIES ON)
if(EMSCRIPTEN)
        add_library(wgpu-backend STATIC 
                ${CMAKE_CURRENT_SOURCE_DIR}/renderer_wgpu.cpp 
                ${CMAKE_CURRENT_SOURCE_DIR}/render_backend_wgpuimpl.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/utils_wgpu.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/bind_group_wgpu.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/dynamic_shadow_array.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/dynamic_light_converter.cpp)
        target_include_directories(wgpu-backend PRIVATE 
                ${CMAKE_SOURCE_DIR}/include 
                ${CMAKE_SOURCE_DIR}/src)

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
                -DDAWN_DEBUG_BREAK_ON_ERROR=1)
        set_property(TARGET webgpu APPEND PROPERTY LINK_DEPENDS 
                ../notes.txt
        )
else()
        if(${SKL_ENABLE_EDITOR_MODE})
                add_library(wgpu-backend STATIC 
                        ${CMAKE_CURRENT_SOURCE_DIR}/renderer_wgpu.cpp 
                        ${CMAKE_CURRENT_SOURCE_DIR}/render_backend_wgpuimpl.cpp 
                        ${CMAKE_CURRENT_SOURCE_DIR}/utils_wgpu.cpp
                        ${imgui_SOURCE_DIR}/backends/imgui_impl_wgpu.cpp
                        ${CMAKE_CURRENT_SOURCE_DIR}/bind_group_wgpu.cpp
                        ${CMAKE_CURRENT_SOURCE_DIR}/dynamic_shadow_array.cpp
                        ${CMAKE_CURRENT_SOURCE_DIR}/dynamic_light_converter.cpp)
                target_include_directories(wgpu-backend PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src $<BUILD_INTERFACE:${imgui_SOURCE_DIR}>)
        else()
                add_library(wgpu-backend STATIC 
                        ${CMAKE_CURRENT_SOURCE_DIR}/renderer_wgpu.cpp 
                        ${CMAKE_CURRENT_SOURCE_DIR}/render_backend_wgpuimpl.cpp
                        ${CMAKE_CURRENT_SOURCE_DIR}/utils_wgpu.cpp
                        ${CMAKE_CURRENT_SOURCE_DIR}/bind_group_wgpu.cpp
                        ${CMAKE_CURRENT_SOURCE_DIR}/dynamic_shadow_array.cpp
                        ${CMAKE_CURRENT_SOURCE_DIR}/dynamic_light_converter.cpp)
                target_include_directories(wgpu-backend PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)
        endif()

        add_library(webgpu INTERFACE)

        target_compile_definitions(wgpu-backend PUBLIC IMGUI_IMPL_WEBGPU_BACKEND_DAWN ON)

        # Fetches Dawn
        # set(DAWN_FETCH_DEPENDENCIES ON)
        # set(DAWN_BUILD_MONOLITHIC_LIBRARY SHARED)
        CPMAddPackage(NAME dawn URL "https://dawn.googlesource.com/dawn/+archive/refs/heads/chromium/7285.tar.gz")
        target_link_libraries(webgpu INTERFACE dawn::webgpu_dawn)
endif()

target_compile_definitions(wgpu-backend PRIVATE SKL_ENABLED_EDITOR=${SKL_ENABLE_EDITOR_MODE})
target_compile_definitions(wgpu-backend PRIVATE SKL_LOGGING_ENABLED=${SKL_ENABLE_LOGGING})

# Includes ported SDL3-webgpu conduit
add_subdirectory(${CMAKE_SOURCE_DIR}/include/webgpu/sdl3webgpu-main sdl3webgpu)

target_link_libraries(wgpu-backend PRIVATE glm::glm sdl3webgpu imgui skl-math)