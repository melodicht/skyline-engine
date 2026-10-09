#pragma once

#include <meta_definitions.h>
#include <skl_math_types.h>
#include <render_types.h>

// The subset of renderer interface used by the game module.

// GPU zone durations in nanoseconds, indexed by zoneNames.
struct RenderFramePerformanceInfo {
    std::vector<uint64_t> zoneTimes;
    std::vector<std::string> zoneNames;
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

struct IconRenderInfo {
    glm::vec3 pos;
    TextureID texture;
    u32 id;
};

struct SceneLightingRenderInfo {
    glm::vec3 ambientLighting{ 0.25 };
    f32 pcfWorldRange;
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

    // WGPU Specific
    f32 falloff; /* WGPU uses same distance falloff equation as point light*/

    bool needsUpdate;
};

struct PointLightRenderInfo {
    LightID lightID;
    Transform3D* transform;

    glm::vec3 diffuse;
    glm::vec3 specular;

    f32 radius;
    f32 falloff;

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

    // Editor stuff
    glm::ivec2 cursorPos;
    std::vector<IconRenderInfo>& icons;

    // Vulkan Specific

    // WGPU Specific
    SceneLightingRenderInfo sceneLighting;
};

// FlushProfilingZones drains completed GPU batches; returns empty when unavailable.
#define RENDERER_FUNCS(method) \
    method(LightID,AddDirLight,())\
    method(LightID,AddSpotLight,())\
    method(LightID,AddPointLight,())\
    method(void,DestroyDirLight,(LightID lightID))\
    method(void,DestroySpotLight,(LightID lightID))\
    method(void,DestroyPointLight,(LightID lightID))\
    method(u32,GetIndexAtCursor,())\
    method(void,RenderUpdate,(RenderFrameInfo& state))\
    method(std::vector<RenderFramePerformanceInfo>,FlushProfilingZones,())
DEFINE_GAME_MODULE_API(PlatformRenderer, RENDERER_FUNCS)
