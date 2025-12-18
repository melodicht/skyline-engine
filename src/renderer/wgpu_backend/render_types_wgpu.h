#pragma once

#include "math/skl_math_types.h"

#include <webgpu/webgpu.h>

#include <glm/fwd.hpp>

#include <array>

// >>> Represents helper structs  <<<
#pragma region misc types
// Simply combines a single texture and texture view
// Does not handle the release of the textures on its own
struct WGPUBackendTexture {
    WGPUTexture m_texture;
    WGPUTextureView m_textureView;
};

// Wraps around the core parts of WGPU to allow core of WGPU to be released last
struct WGPUCore {
    WGPUDevice m_device{ };
    WGPUInstance m_instance{ };

    ~WGPUCore() {
        wgpuDeviceRelease(m_device);
        wgpuInstanceRelease(m_instance);
    }
};

// Represents location of a specified mesh within the WebGPU renderer
struct WGPUBackendMeshIdx {
    u32 m_baseIndex{ 0 };
    u32 m_baseVertex{ 0 };
    u32 m_indexCount{ 0 };
    u32 m_vertexCount{ 0 };

    WGPUBackendMeshIdx() : 
        m_baseIndex(),
        m_baseVertex(),
        m_indexCount(),
        m_vertexCount()
    {}

    WGPUBackendMeshIdx(u32 baseIndex, u32 baseVertex, u32 indexCount, u32 vertexCount) : 
        m_baseIndex(baseIndex),
        m_baseVertex(baseVertex),
        m_indexCount(indexCount),
        m_vertexCount(vertexCount)
    {}
};
#pragma endregion

// >>> Represents types meant to be plugged directly into the buffer <<<
#pragma region cpu->gpu types

// TODO: better represent vec3 that are upgraded to vec4 solely for alignment

// Slots all fixed length data into one struct for efficiency
struct WGPUBackendColorPassFixedData
{
    // Represents camera data
    glm::mat4 m_combined;
    glm::mat4 m_view;
    glm::mat4 m_proj;
    glm::vec3 m_pos;

    // Represents light 
    u32 m_dirLightCount;
    u32 m_pointLightCount;
    u32 m_spotLightCount;
    u32 m_dirLightCascadeCount;
    u32 m_padding2;
};

struct WGPUBackendPointDepthPassFixedData
{
    glm::vec3 m_lightPos;
    f32 m_farPlane;
};

// Represents a instance of a mesh
struct WGPUBackendObjectData {
    glm::mat4 m_model;
    glm::mat4x4 m_normMat;
    glm::vec4 m_color;
};

// Represents a single shadowed directional light
struct WGPUBackendDynamicShadowedDirLightData {
    glm::vec3 m_diffuse;
    float m_padding; // Fill with useful stuff later
    glm::vec3 m_specular;
    float m_padding2; // Fill with useful stuff later
    glm::vec3 m_direction;
    float m_intensity;
};

// Represents a single shadowed point light
struct WGPUBackendDynamicShadowedPointLightData {
    glm::vec3 m_diffuse;
    float m_constant;
    glm::vec3 m_specular;
    float m_linear;
    glm::vec3 m_position;
    float m_quadratic;
    float m_distanceCutoff;
    float m_padding1;
    float m_padding2;
    float m_padding3;
};

// Represents a single shadowed spot light
struct WGPUBackendDynamicShadowedSpotLightData {
    glm::vec3 m_diffuse;
    float m_penumbraCutoff;
    glm::vec3 m_specular;
    float m_outerCutoff;
    glm::vec3 m_position;
    float m_padding; // Fill with useful stuff later
    glm::vec3 m_direction;
    float m_padding2; // Fill with useful stuff later
};

#pragma endregion
