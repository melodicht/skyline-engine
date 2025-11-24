#version 460

#extension GL_EXT_scalar_block_layout: require

struct ObjectData
{
    mat4 model;
    int texID;
    mat4 color;
};

struct DirLightData
{
    vec3 direction;
    int shadowID;

    vec3 diffuse;
    vec3 specular;
};

struct SpotLightData
{
    mat4 lightSpace;

    vec3 position;
    vec3 direction;
    int shadowID;

    vec3 diffuse;
    vec3 specular;

    float innerCutoff;
    float outerCutoff;
    float range;
};

struct PointLightData
{
    vec3 position;
    int shadowID;

    vec3 diffuse;
    vec3 specular;

    float constant;
    float linear;
    float quadratic;

    float maxRange;
};

struct LightCascade
{
    mat4 lightSpace;
    float maxDepth;
};

layout (buffer_reference, scalar) readonly buffer ObjectBuffer
{
    ObjectData objects[];
};

layout (buffer_reference, scalar) readonly buffer DirLightBuffer
{
    DirLightData dirLights[];
};

layout (buffer_reference, scalar) readonly buffer SpotLightBuffer
{
    SpotLightData spotLights[];
};

layout (buffer_reference, scalar) readonly buffer PointLightBuffer
{
    PointLightData pointLights[];
};

layout (buffer_reference, scalar) readonly buffer LightCascadeBuffer
{
    LightCascade cascades[];
};

layout (push_constant, scalar) uniform PushConstants
{
    layout (offset = 16) ObjectBuffer objectBuffer;
    DirLightBuffer dirLightBuffer;
    LightCascadeBuffer dirCascadeBuffer;
    SpotLightBuffer spotLightBuffer;
    PointLightBuffer pointLightBuffer;
    uint dirLightCount;
    uint dirCascadeCount;
    uint spotLightCount;
    uint pointLightCount;
    vec3 ambientLight;
} pcs;

layout(location = 0) in vec3 worldPos;
layout(location = 1) in vec3 normal;
layout(location = 2) in float uvX;
layout(location = 3) in vec3 eyeRelPos;
layout(location = 4) in float uvY;
layout(location = 5) in int instance;

layout(set = 0, binding = 0) uniform texture2D textures[];
layout(set = 0, binding = 0) uniform texture2DArray arrayTextures[];
layout(set = 0, binding = 0) uniform textureCube cubemaps[];

layout(set = 0, binding = 1) uniform samplerShadow shadowSampler;

layout(set = 0, binding = 2) uniform sampler textureSampler;

void main()
{
    
}