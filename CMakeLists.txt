cmake_minimum_required(VERSION 3.22)
project("skyline-engine")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# TODO(marvin): Work towards getting this out by doing things properly
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)


set(PLATFORM_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)
set(GAME_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/game.cpp)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake)

#==============================================================================
# COMPILATION OPTIONS
#==============================================================================

add_compile_definitions(NOMINMAX)

# option_if_not_defined vs option
# https://github.com/google/marl/issues/223

# Determines which rendering backend is used
if(NOT DEFINED SKL_RENDER_SYS)
        set(SKL_RENDER_SYS "Default" CACHE STRING "Which graphics API should the rendering backend be based on")
        set_property(CACHE SKL_RENDER_SYS PROPERTY STRINGS "Default" "WebGPU" "Vulkan")
endif()

if(NOT DEFINED SKL_ENABLE_EDITOR_MODE)
        if(${EMSCRIPTEN})
                option(SKL_ENABLE_EDITOR_MODE "Whether editor is enabled" OFF)
                if(${SKL_EDITOR_MODE})
                        message(FATAL_ERROR "Editor functionality should not be active with emscripten")
                endif()
        else()
                option(SKL_ENABLE_EDITOR_MODE "Whether editor is enabled" ON)
        endif()
endif()

# Determines whether debugging functionality is enabled
if(NOT DEFINED SKL_ENABLE_DEBUGGING)
        option(SKL_ENABLE_DEBUGGING "Whether debug functionality is enabled" ON)
endif()

# When debugging mode is active, determines whether logging should also be enabled.
if(NOT DEFINED SKL_ENABLE_LOGGING)
        if(${SKL_ENABLE_DEBUGGING})
                option(SKL_ENABLE_LOGGING "Whether logging should be enabled or not" ON)
        elseif(NOT ${SKL_ENABLE_DEBUGGING})
                option(SKL_ENABLE_LOGGING "Whether logging should be enabled or not" OFF)
                message(FATAL_ERROR "Logging cannot be turned on for non debugging builds")
        endif()
endif()

# When debugging mode is active, determines whether hard stopping asserts should also be enabled.
# Currently not implemented.
if(NOT DEFINED SKL_ENABLE_ASSERTS)
        if(${SKL_ENABLE_DEBUGGING})
                option(SKL_ENABLE_ASSERTS "Whether logging should be enabled or not" ON)
        elseif(NOT ${SKL_ENABLE_DEBUGGING})
                option(SKL_ENABLE_ASSERTS "Whether logging should be enabled or not" OFF)
                if(${SKL_ENABLE_ASSERTS})
                        message(FATAL_ERROR "Logging cannot be turned on for non debugging builds")
                endif()
        endif()
endif()

# Assumes a development build when not specified otherwise
if(NOT DEFINED SKL_INTERNAL)
        set(SKL_INTERNAL 1)
endif()
if(NOT DEFINED SKL_SLOW)
        set(SKL_SLOW 1)
endif()

add_library(COMPILE_OPTIONS INTERFACE)
target_compile_definitions(COMPILE_OPTIONS INTERFACE
        SKL_ENABLED_EDITOR=${SKL_ENABLE_EDITOR_MODE}
        SKL_LOGGING_ENABLED=${SKL_ENABLE_LOGGING}
        SKL_INTERNAL=${SKL_INTERNAL}
        SKL_SLOW=${SKL_SLOW})

#==============================================================================
# PACKAGE MANAGER
#==============================================================================

set(CPM_SOURCE_CACHE "${CMAKE_SOURCE_DIR}/build_external")
CPMAddPackage("gh:libsdl-org/SDL#release-3.2.20")
CPMAddPackage("gh:g-truc/glm#1.0.1")
CPMAddPackage("gh:spnda/fastgltf#v0.8.0")
CPMAddPackage("gh:ocornut/imgui#v1.91.9b")
CPMAddPackage("gh:marzer/tomlplusplus#v3.4.0")

CPMAddPackage(
    NAME JoltPhysics
    GITHUB_REPOSITORY jrouwe/JoltPhysics
    GIT_TAG v5.3.0
    SOURCE_SUBDIR Build
    OPTIONS
        "TARGET_UNIT_TESTS OFF"
        "TARGET_HELLO_WORLD OFF"
        "TARGET_PERFORMANCE_TEST OFF"
        "TARGET_SAMPLES OFF"
        "ENABLE_ALL_WARNINGS OFF"
      )

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/math skl-math)

#==============================================================================
# IMGUI
#==============================================================================

file(GLOB imgui_sources ${imgui_SOURCE_DIR}/*.cpp)
add_library(imgui SHARED ${imgui_sources})
target_include_directories(imgui PUBLIC $<BUILD_INTERFACE:${imgui_SOURCE_DIR}>)
list(APPEND PLATFORM_SRC ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp)

#==============================================================================
# SHARED DEPENDENCIES (Between platform, rendering backend, and game)
#==============================================================================

add_library(SHARED_DEPENDENCIES INTERFACE)
target_link_libraries(SHARED_DEPENDENCIES INTERFACE
        SDL3::SDL3
        glm::glm
        imgui
        skl-math
        tomlplusplus::tomlplusplus
        COMPILE_OPTIONS
)

add_library(RENDERING_BACKEND INTERFACE)

#==============================================================================
# RENDERING BACKEND
#==============================================================================

# Determines rendering backend if none is explicitly picked
if(${SKL_RENDER_SYS} STREQUAL "Default")
        message(STATUS "Assigning rendering backend based on machine...")

        # Checks if Vulkan driver can be found on system
        find_package(Vulkan QUIET)
        if(Vulkan_FOUND AND NOT APPLE)
                # Likely Vulkan will run better on the things it is natively supported by
                set(SKL_RENDER_SYS "Vulkan")
        else()
                # Otherwise run webgpu backend
                set(SKL_RENDER_SYS "WebGPU")
        endif()

# Checks if selected option is available for current device, throws error if not
else()
        message(STATUS "Checking for api support on current machine...")
        # Web simply does not work with anything outside of WebGPU
        if(EMSCRIPTEN)
                if(NOT ${SKL_RENDER_SYS} STREQUAL "WebGPU")
                        message(FATAL_ERROR "Web builds need to use WebGPU backend")
                endif()
        endif()
endif()

# Accordingly resolves dependencies required for each rendering backend
message(STATUS "Initializing [${SKL_RENDER_SYS}] rendering backend...")
if(${SKL_RENDER_SYS} STREQUAL "Vulkan")
        add_definitions(-DIMGUI_IMPL_VULKAN_USE_VOLK)
        if (APPLE)
                CPMAddPackage(
                        NAME MoltenVK
                        URL "https://github.com/KhronosGroup/MoltenVK/releases/download/v1.4.1/MoltenVK-all.tar")
          
                set(ENV{VULKAN_SDK} "${MoltenVK_SOURCE_DIR}/MoltenVK")
                set(Vulkan_LIBRARY "${MoltenVK_SOURCE_DIR}/MoltenVK/dylib/macOS/libMoltenVK.dylib")
                set(Vulkan_INCLUDE_DIR "${MoltenVK_SOURCE_DIR}/MoltenVK/include")
        endif()
        find_package(Vulkan REQUIRED)
        set(VULKAN_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer/vk_backend)
        add_library(vulkan-backend SHARED
                ${VULKAN_SRC_DIR}/renderer_vk.cpp
                ${VULKAN_SRC_DIR}/VkBootstrap.cpp
                ${VULKAN_SRC_DIR}/imgui_impl_vulkan.cpp)
        target_link_libraries(vulkan-backend PRIVATE
                SHARED_DEPENDENCIES
                Vulkan::Headers)
        if (APPLE)
            target_link_libraries(vulkan-backend PRIVATE Vulkan::Vulkan)
        endif()
        target_include_directories(vulkan-backend PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/include
                ${CMAKE_CURRENT_SOURCE_DIR}/include/vulkan
                ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_compile_definitions(vulkan-backend PRIVATE VK_NO_PROTOTYPES)
        target_compile_definitions(RENDERING_BACKEND INTERFACE
                SKL_RENDERER=0
        )
        target_link_libraries(RENDERING_BACKEND INTERFACE vulkan-backend)
elseif(${SKL_RENDER_SYS} STREQUAL "WebGPU")
        # Includes backend dll as part of projects
        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/src/renderer/wgpu_backend wgpu-backend)
        # Allows for switching off certain pieces of logic not supported by certain backends
        target_compile_definitions(RENDERING_BACKEND INTERFACE 
                SKL_RENDERER=1
        )
        target_link_libraries(RENDERING_BACKEND INTERFACE wgpu-backend)
endif()

#==============================================================================
# GAME MODULE
#==============================================================================
add_library(game-module MODULE ${GAME_SRC})
# Version the PDB file to avoid locking issues
string(TIMESTAMP BUILD_TIME "%Y%m%d_%H%M%S")
set_target_properties(game-module PROPERTIES
        PDB_NAME "game_module_${BUILD_TIME}")
target_compile_definitions(game-module PRIVATE
        _CRT_SECURE_NO_WARNINGS
        debugRecordArray=debugRecordsGame
        SKL_GAME_MODULE)
target_link_libraries(game-module PRIVATE
        SHARED_DEPENDENCIES
        Jolt)
target_include_directories(game-module PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${JoltPhysics_SOURCE_DIR})

set(PCH_HEADERS
        <iostream>
        <string>
        <vector>
        <glm/glm.hpp>
        <glm/gtc/type_ptr.hpp>
        <Jolt/Jolt.h>
        <Jolt/Core/JobSystemThreadPool.h>
        <Jolt/Physics/PhysicsSystem.h>
        # TODO(marvin): Precompiling imgui header causes problems.
        # <imgui.h>
)

if(WIN32)
        list(APPEND PCH_HEADERS <windows.h>)
endif()

target_precompile_headers(game-module PRIVATE ${PCH_HEADERS})

#==============================================================================
# PLATFORM
#==============================================================================
add_executable(platform ${PLATFORM_SRC})
set_target_properties(platform PROPERTIES
        OUTPUT_NAME ${PROJECT_NAME}
)
target_include_directories(platform PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        $<BUILD_INTERFACE:${imgui_SOURCE_DIR}>)
target_link_libraries(platform PRIVATE
        SHARED_DEPENDENCIES
        RENDERING_BACKEND
        fastgltf::fastgltf)

add_dependencies(platform game-module)

if (EMSCRIPTEN)
        set_target_properties(platform PROPERTIES SUFFIX ".html")
elseif (WIN32)
        add_custom_command(TARGET platform POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:platform> $<TARGET_FILE_DIR:platform>
                COMMAND_EXPAND_LISTS)
endif()

#==============================================================================
# POST-BUILD
#==============================================================================

# Deals with local compilation
if(NOT EMSCRIPTEN)
        # Represents the directory the resulting executable will be at
        # Depending on build system this may be shifted
        set(BUILD_DIR ${CMAKE_BINARY_DIR})

        if(XCODE)
                # This makes the assumption that if SKL_ENABLE_DEBUGGING is true, xcode is also running in debug mode
                # This behavior needs to be enforced by XCODE
                if(SKL_ENABLE_DEBUGGING)
                       set(BUILD_DIR ${CMAKE_BINARY_DIR}/Debug)
                endif()
        endif()

        set(SHADERS_IN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
        set(SHADERS_OUT_DIR "${BUILD_DIR}/shaders")
        file(MAKE_DIRECTORY ${SHADERS_OUT_DIR})

        # Includes shaders in build
        if(SKL_RENDER_SYS STREQUAL "Vulkan")
                set(GLSLANG "glslangValidator")

                macro(add_shader inName outName)
                    set(SHADER_IN_PATH "${SHADERS_IN_DIR}/vulkan/${inName}.glsl")
                    set(SHADER_OUT_PATH "${SHADERS_OUT_DIR}/${outName}.spv")
                    list(APPEND SHADER_OUT_NAMES ${SHADER_OUT_PATH})
                    add_custom_command(
                            MAIN_DEPENDENCY ${SHADER_IN_PATH}
                            OUTPUT ${SHADER_OUT_PATH}
                            COMMAND ${GLSLANG} ${SHADER_IN_PATH} -o ${SHADER_OUT_PATH} --target-env vulkan1.3 ${ARGN}
                            VERBATIM)
                endmacro()

                add_shader("color.vert" "depth.vert")
                add_shader("color.vert" "shadow.vert" -DWORLDPOS)
                add_shader("color.vert" "color.vert" -DWORLDPOS -DVERTATTR)
                add_shader("color.frag" "color.frag")
                add_shader("color.frag" "editor.frag" -DEDITOR)
                add_shader("shadow.frag" "shadow.frag")
                add_shader("dirshadow.frag" "dirshadow.frag")
                add_shader("icon.vert" "icon.vert")
                add_shader("icon.frag" "icon.frag")

                add_custom_target(build_shaders DEPENDS ${SHADER_OUT_NAMES})

                add_dependencies(platform build_shaders)
        elseif(SKL_RENDER_SYS STREQUAL "WebGPU")
                add_custom_command(TARGET platform PRE_LINK
                        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/shaders/webgpu" "${BUILD_DIR}/shaders"
                )
        endif()

        # Imports other resources into build
        add_custom_target(copy_data ${CMAKE_COMMAND} -E copy_directory_if_different "${CMAKE_CURRENT_SOURCE_DIR}/data" ${BUILD_DIR})

        add_dependencies(platform copy_data)

# Preloads files for emscripten
else()
        target_link_options(platform PRIVATE
                "SHELL:--preload-file ${CMAKE_SOURCE_DIR}/shaders/webgpu@/shaders/" #This isn't great, would be better to encapsulate all of this into a specific folder
                "SHELL:--preload-file ${CMAKE_SOURCE_DIR}/data/models@/models/"
                "SHELL:--preload-file ${CMAKE_SOURCE_DIR}/data/fonts@/fonts/"
                --preload-plugins
        )
endif()
