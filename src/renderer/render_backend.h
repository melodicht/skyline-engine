#pragma once

#include "math/skl_math_types.h"
#include "render_game.h"
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

    bool editor;

    // Vulkan Specific 

    // WGPU Specific
};
void InitRenderer(RenderInitInfo& info);

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

// Moves a texture to the GPU,
struct RenderUploadTextureInfo {
    // Shared
    u32 width;
    u32 height;
    u32* pixelData;
};
TextureID UploadTexture(RenderUploadTextureInfo& info);

// Moves a texture to the renderer, immediately replacing Skybox with new one
// Assumes that all given images will have same dimensions
struct RenderSetSkyboxInfo {
    // Shared
    u32 width;
    u32 height;
    // In order of posX, negX, posY, negY, posZ, negZ
    std::array<u32*,6> cubemapData;
};
void SetSkyboxTexture(RenderSetSkyboxInfo& info);

// Destroy the mesh at the given MeshID
struct RenderDestroyMeshInfo {
    // Shared 
    MeshID meshID;

    // Vulkan Specific

    // WGPU Specific
};
void DestroyMesh(RenderDestroyMeshInfo& info);