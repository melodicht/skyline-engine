#pragma once

#include "renderer/render_types.h"

// The subset of renderer interface used by the game module.

// Set up the render pipelines
struct RenderPipelineInitInfo {
    // Shared 

    // Vulkan Specific

    // WGPU Specific
};

struct MeshRenderInfo {
    // Shared
    glm::mat4 matrix;
    glm::vec3 rgbColor;
    MeshID mesh;
    TextureID texture;
    u32 id;

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

    glm::ivec2 cursorPos;

    // Vulkan Specific

    // WGPU Specific
};

#define RENDERER_METHODS(method) \
    method(void, InitPipelines, (RenderPipelineInitInfo& info))\
    method(LightID,AddDirLight,())\
    method(LightID,AddSpotLight,())\
    method(LightID,AddPointLight,())\
    method(void,DestroyDirLight,(LightID lightID))\
    method(void,DestroySpotLight,(LightID lightID))\
    method(void,DestroyPointLight,(LightID lightID))\
    method(u32,GetIndexAtCursor,())\
    method(void,RenderUpdate,(RenderFrameInfo& state))
DEFINE_GAME_MODULE_API(PlatformRenderer, RENDERER_METHODS)
