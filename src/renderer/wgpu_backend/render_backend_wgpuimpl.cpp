#include "renderer/render_backend.h"

#include <map>
#include <random>

#include "renderer/wgpu_backend/renderer_wgpu.h"


// This is done to force encapsulation of the wgpu renderer and renderer types
static WGPURenderBackend wgpuRenderer;

SDL_WindowFlags GetRenderWindowFlags() {
    return 0;
}

void InitRenderer(RenderInitInfo& desc) {
    wgpuRenderer.InitRenderer(desc.window, desc.startWidth, desc.startHeight);
}

void InitPipelines(RenderPipelineInitInfo& desc) {
    wgpuRenderer.InitPipelines();
}

MeshID UploadMesh(RenderUploadMeshInfo& desc) {
    return wgpuRenderer.UploadMesh(desc.vertSize, desc.vertData, desc.idxSize, desc.idxData);
}

TextureID UploadTexture(RenderUploadTextureInfo& desc) {
        return 0;
}

void SetSkyboxTexture(RenderSetSkyboxInfo& info) {
    wgpuRenderer.SetSkybox(
        info.width, 
        info.height, 
        info.cubemapData);
}

void DestroyMesh(RenderDestroyMeshInfo& desc) {
    wgpuRenderer.DestroyMesh(desc.meshID);
}

// This compiles information from scene to be plugged into renderer
void RenderUpdate(RenderFrameInfo& state) {
    wgpuRenderer.RenderUpdate(state);
}

LightID AddDirLight() {
    return wgpuRenderer.AddDirLight();
}
LightID AddSpotLight() {
    return wgpuRenderer.AddSpotLight();
}
LightID AddPointLight() {
    return wgpuRenderer.AddPointLight();
}

void DestroyDirLight(LightID lightID) {
    wgpuRenderer.DestroyDirLight(lightID);
}
void DestroySpotLight(LightID lightID) {
    wgpuRenderer.DestroySpotLight(lightID);
}
void DestroyPointLight(LightID lightID) {
    wgpuRenderer.DestroyPointLight(lightID);
}

