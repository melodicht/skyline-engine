#pragma once

#include "math/skl_math_types.h"
#include "render_types.h"
#include "render_game.h"

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

PLATFORM_RENDERER_INIT_PIPELINES(InitPipelines);

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

PLATFORM_RENDERER_ADD_LIGHT(AddDirLight);
PLATFORM_RENDERER_ADD_LIGHT(AddSpotLight);
PLATFORM_RENDERER_ADD_LIGHT(AddPointLight);

PLATFORM_RENDERER_DESTROY_LIGHT(DestroyDirLight);
PLATFORM_RENDERER_DESTROY_LIGHT(DestroySpotLight);
PLATFORM_RENDERER_DESTROY_LIGHT(DestroyPointLight);

PLATFORM_RENDERER_GET_INDEX_AT_CURSOR(GetIndexAtCursor);

// Destroy the mesh at the given MeshID
struct RenderDestroyMeshInfo {
    // Shared 
    MeshID meshID;

    // Vulkan Specific

    // WGPU Specific
};
void DestroyMesh(RenderDestroyMeshInfo& info);

// Renders a frame using the supplied render state
// The driving function of the entire renderer.
PLATFORM_RENDERER_RENDER_UPDATE(RenderUpdate);
