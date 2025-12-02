cmake_minimum_required(VERSION 3.17)
project("skyline-engine")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# TODO(marvin): Work towards getting this out by doing things properly
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)


set(PLATFORM_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)
set(GAME_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/game.cpp)

include(cmake/CPM.cmake)
CPMAddPackage("gh:libsdl-org/SDL#release-3.2.12")
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
# SHARED DEPENDENCIES (Between platform, rendering backend, and game)
#==============================================================================

add_library(SHARED_DEPENDENCIES INTERFACE)
target_link_libraries(SHARED_DEPENDENCIES INTERFACE
        SDL3::SDL3
        glm::glm
        imgui
        skl-math
        tomlplusplus::tomlplusplus
)

add_library(RENDERING_BACKEND INTERFACE)

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
                option(SKL_ENABLE_EDITOR_MODE "Whether editor " OFF)
                if(${SKL_EDITOR_MODE})
                        message(FATAL_ERROR "Editor functionality should not be active with emscripten")
                endif()
        else()
                option(SKL_ENABLE_EDITOR_MODE "Whether editor " ON)
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

target_compile_definitions(SHARED_DEPENDENCIES
        INTERFACE SKL_ENABLED_EDITOR=${SKL_ENABLE_EDITOR_MODE}
        INTERFACE SKL_LOGGING_ENABLED=${SKL_ENABLE_LOGGING}
        INTERFACE SKL_INTERNAL=${SKL_INTERNAL})

#==============================================================================
# IMGUI
#==============================================================================

file(GLOB imgui_sources ${imgui_SOURCE_DIR}/*.cpp)
add_library(imgui SHARED ${imgui_sources})
target_include_directories(imgui PRIVATE $<BUILD_INTERFACE:${imgui_SOURCE_DIR}>)
list(APPEND PLATFORM_SRC ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp)

#==============================================================================
# RENDERING BACKEND
#==============================================================================

# Determines rendering backend if none is explicitly picked
if(${SKL_RENDER_SYS} STREQUAL "Default")
        message("Assigning rendering backend based on machine...")
        if(APPLE)
                # Currently MoltenVK has not caught up in terms of SDK to the currently used vulkan version
                # Maybe we should change once that becomes avaliable
                set(SKL_RENDER_SYS "WebGPU")
        elseif(EMSCRIPTEN)
                # No other options for web
                set(SKL_RENDER_SYS "WebGPU")
        else()
                # Likely Vulkan will run better on the things it is natively supported by
                set(SKL_RENDER_SYS "Vulkan")
        endif()

# Checks if selected option is available for current device, throws error if not
else()
        message("Checking for api support on current machine...")
        # Web simply does not work with anything outside of WebGPU
        if(EMSCRIPTEN)
                if(NOT ${SKL_RENDER_SYS} STREQUAL "WebGPU")
                        message(FATAL_ERROR "Web builds need to use WebGPU backend")
                endif()
        endif()
endif()

# Accordingly resolves dependencies required for each rendering backend
message("Initializing [${SKL_RENDER_SYS}] rendering backend...")
if(${SKL_RENDER_SYS} STREQUAL "Vulkan")
        add_definitions(-DIMGUI_IMPL_VULKAN_USE_VOLK)
        find_package(Vulkan REQUIRED)
        set(VULKAN_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer/vk_backend)
        add_library(vulkan-backend SHARED
                ${VULKAN_SRC_DIR}/renderer_vk.cpp
                ${VULKAN_SRC_DIR}/VkBootstrap.cpp
                ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp)
        target_link_libraries(vulkan-backend PRIVATE
                SHARED_DEPENDENCIES
                Vulkan::Headers)
        if (APPLE)
            target_link_libraries(vulkan-backend PRIVATE Vulkan::Vulkan)
        endif()
        target_include_directories(vulkan-backend PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/include
                ${CMAKE_CURRENT_SOURCE_DIR}/include/vulkan
                ${CMAKE_CURRENT_SOURCE_DIR}/src
                $<BUILD_INTERFACE:${imgui_SOURCE_DIR}>)
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
        debugRecordArray=debugRecordsGame)
target_link_libraries(game-module PRIVATE
        SHARED_DEPENDENCIES
        RENDERING_BACKEND
        Jolt)
target_include_directories(game-module PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${JoltPhysics_SOURCE_DIR})

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
                file(GLOB_RECURSE SHADERS "${SHADERS_IN_DIR}/*.slang")

                set(SLANGC "slangc")

                foreach(SHADER ${SHADERS})
                get_filename_component(SHADER_NAME ${SHADER} NAME_WE)
                set(SHADER_OUT_NAME "${SHADERS_OUT_DIR}/${SHADER_NAME}.spv")
                list(APPEND SHADER_OUT_NAMES ${SHADER_OUT_NAME})
                add_custom_command(
                        MAIN_DEPENDENCY ${SHADER}
                        OUTPUT ${SHADER_OUT_NAME}
                        COMMAND ${SLANGC} ${SHADER} -o ${SHADER_OUT_NAME} -target spirv -fvk-use-entrypoint-name -emit-spirv-directly -fvk-use-scalar-layout -matrix-layout-row-major
                        VERBATIM)
                endforeach()

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
