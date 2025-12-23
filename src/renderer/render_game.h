#pragma once

#include "math/skl_math_types.h"

// The subset of renderer interface used by the game module.

// Set up the render pipelines
struct RenderPipelineInitInfo {
    // Shared 

    // Vulkan Specific

    // WGPU Specific
};
#define PLATFORM_RENDERER_INIT_PIPELINES(proc) void proc(RenderPipelineInitInfo& info)
typedef PLATFORM_RENDERER_INIT_PIPELINES(platform_renderer_init_pipelines_t);

#define PLATFORM_RENDERER_ADD_LIGHT(proc) LightID proc()
typedef PLATFORM_RENDERER_ADD_LIGHT(platform_renderer_add_light_t);

#define PLATFORM_RENDERER_DESTROY_LIGHT(proc) void proc(LightID lightID)
typedef PLATFORM_RENDERER_DESTROY_LIGHT(platform_renderer_destroy_light_t);

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

    // Vulkan Specific

    // WGPU Specific
};
#define PLATFORM_RENDERER_RENDER_UPDATE(proc) void proc(RenderFrameInfo& info)
typedef PLATFORM_RENDERER_RENDER_UPDATE(platform_renderer_render_update_t);
