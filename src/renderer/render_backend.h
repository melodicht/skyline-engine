#pragma once

#include "math/skl_math_types.h"
#include "render_types.h"

#include <SDL3/SDL.h>

#include <map>

// Common interface between renderers for systems to call.
// The interfaces take in Info objects in order to allow for 
// updates to the inputs of the interface without updates of everything that uses the interface .
// Assumes that a SDL3 surface is being drawn upon.

// Get the flags that should be added onto the SDL window creation to support this backend
SDL_WindowFlags GetRenderWindowFlags();

// Sets a SDL window to draw to and initializes the back end
struct RenderInitInfo {
    // Shared 
    SDL_Window *window;
    u32 startWidth;
    u32 startHeight;

    // Vulkan Specific 

    // WGPU Specific
};
void InitRenderer(RenderInitInfo& info);

// Set up the render pipelines
struct RenderPipelineInitInfo {
    // Shared 

    // Vulkan Specific

    // WGPU Specific
};
void InitPipelines(RenderPipelineInitInfo& info);

// Moves a mesh to the GPU,
// Returns a uint that represents the mesh's ID
struct RenderUploadMeshInfo {
    // Shared 
    Vertex* vertData;
    u32* idxData;
    u32 vertSize;
    u32 idxSize;

    // Vulkan Specific

    // WGPU Specific
};
MeshID UploadMesh(RenderUploadMeshInfo& info);

struct RenderUploadTextureInfo {
    u32 width;
    u32 height;
    u32* pixelData;
};

TextureID UploadTexture(RenderUploadTextureInfo& info);

LightID AddDirLight();
LightID AddSpotLight();
LightID AddPointLight();

void DestroyDirLight(LightID lightID);
void DestroySpotLight(LightID lightID);
void DestroyPointLight(LightID lightID);

// Destroy the mesh at the given MeshID
struct RenderDestroyMeshInfo {
    // Shared 
    MeshID meshID;

    // Vulkan Specific

    // WGPU Specific
};
void DestroyMesh(RenderDestroyMeshInfo& info);

struct MeshRenderInfo {
    // Shared
    glm::mat4 matrix;
    glm::vec3 rgbColor;
    MeshID mesh;
    TextureID texture;

    // Vulkan Specific

    // WGPU Specific
};

struct DirLightRenderInfo {
    // Shared
    LightID lightID;
    Transform3D* transform;

    glm::vec3 diffuse;
    glm::vec3 specular;

    // Vulkan Specific

    // WGPU Specific
};

struct SpotLightRenderInfo {
    LightID lightID;
    Transform3D* transform;

    glm::vec3 diffuse;
    glm::vec3 specular;

    f32 innerCone;
    f32 outerCone;
    f32 range;

    bool needsUpdate;
};

struct PointLightRenderInfo {
    LightID lightID;
    Transform3D* transform;

    glm::vec3 diffuse;
    glm::vec3 specular;

    f32 constant;
    f32 linear;
    f32 quadratic;

    f32 maxRange;

    bool needsUpdate;
};

// Represents the information needed to render a single frame on any renderer
struct RenderFrameInfo {
    // Shared
    Transform3D* cameraTransform;
    std::vector<MeshRenderInfo> &meshes;

    std::vector<DirLightRenderInfo>& dirLights;
    std::vector<SpotLightRenderInfo>& spotLights;
    std::vector<PointLightRenderInfo>& pointLights;

    float cameraFov;
    float cameraNear;
    float cameraFar;

    // Vulkan Specific

    // WGPU Specific
};

// Renders a frame using the supplied render state
// The driving function of the entire renderer.
void RenderUpdate(RenderFrameInfo& info);