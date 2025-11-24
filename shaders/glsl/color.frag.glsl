#version 460

#extension GL_EXT_scalar_block_layout: require

struct CameraData
{
    mat4 view;
    mat4 proj;
    vec3 pos;
};

struct ObjectData
{
    mat4 model;
    int texID;
    mat4 color;
};

struct Vertex
{
    vec3 pos;
    float uvX;
    vec3 normal;
    float uvY;
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

layout (buffer_reference, scalar) readonly buffer CameraBuffer
{
    CameraData camera;
};

layout (buffer_reference, scalar) readonly buffer ObjectBuffer
{
    ObjectData objects[];
};

layout (buffer_reference, scalar) readonly buffer VertexBuffer
{
    Vertex vertices[];
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
    CameraBuffer cameraBuffer;
    ObjectBuffer objectBuffer;
    VertexBuffer vertexBuffer;
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

layout(location = 0) in vec4 worldPos;
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
    vec4 viewPos = pcs.cameraBuffer.camera.view * worldPos;

    vec3 light = pcs.ambientLight;

    ObjectData object = pcs.objectBuffer.objects[instance];

    mat4 color = object.color;

    if (object.texID != -1)
    {
        color *= texture(sampler2D(textures[object.texID], textureSampler), vec2(uvX, uvY));
    }

    for (uint i = 0; i < pcs.dirLightCount; i++)
    {
        DirLightData dirLight = pcs.dirLightBuffer.dirLights[i];

        uint cascadeCount = pcs.dirCascadeCount;
        uint firstCascade = i * cascadeCount;
        uint lastCascade = ((i + 1) * cascadeCount);

        uint cascade = lastCascade;
        for (uint c = firstCascade; c < lastCascade; c++)
        {
            if (viewPos.z < pcs.dirCascadeBuffer.cascades[c].maxDepth)
            {
                cascade = c;
                break;
            }
        }

        vec4 lightRelPos = pcs.dirCascadeBuffer.cascades[cascade].lightSpace *
            (worldPos + vec4(normal * 8, 0.0));

        vec3 lightPosNorm = (lightRelPos.xyz / lightRelPos.w);
        vec3 lightPosScaled = vec3(lightPosNorm.xy * 0.5 + 0.5, lightPosNorm.z);

        float unshadowed = 0;

        for (int x = -1; x <= 1; x++)
        {
            for (int y = -1; y <= 1; y++)
            {
                unshadowed += texture(sampler2DArrayShadow(textures[dirLight.shadowID], shadowSampler),
                    vec3(lightPosScaled.xy, cascade), lightPosScaled.z, vec2(x, y)) / 9;
            }
        }

        vec3 diffuse = max(dot(normal, -dirLight.direction), 0.0) * dirLight.diffuse;
        vec3 viewDir = normalize(-eyeRelPos);
        vec3 halfDir = normalize(viewDir - dirLight.direction);
        vec3 specular = pow(max(dot(normal, halfDir), 0.0), 32.0) * dirLight.specular;

        light += unshadowed * (diffuse + specular);
    }

    for (uint i = 0; i < pcs.spotLightCount; i++)
    {
        SpotLightData spotLight = pcs.spotLights[i];

        float4 lightRelPos = mul(vertData.worldPos + float4(vertData.normal * 8, 0.0), spotLight.lightSpace);

        float3 normal = vertData.normal;

        float3 lightPosNorm = (lightRelPos.xyz / lightRelPos.w);
        float3 lightPosScaled = float3(lightPosNorm.xy * 0.5 + 0.5, lightPosNorm.z);

        Texture2D map = textures[spotLight.shadowID];

        float unshadowed = 0;

        for (int x = -1; x <= 1; x++)
        {
            for (int y = -1; y <= 1; y++)
            {
                unshadowed += map.SampleCmp(shadowSampler, lightPosScaled.xy, lightPosScaled.z, int2(x, y)) / 9;
            }
        }

        float3 lightDir = normalize(vertData.worldPos.xyz - spotLight.position);
        float lightAngle = dot(lightDir, spotLight.direction);

        float fadeSize = spotLight.innerCutoff - spotLight.outerCutoff;
        float intensity = clamp((lightAngle - spotLight.outerCutoff) / fadeSize, 0.0, 1.0);

        float3 diffuse = max(dot(normal, -lightDir), 0.0) * spotLight.diffuse;
        float3 viewDir = normalize(-vertData.eyeRelPos);
        float3 halfDir = normalize(viewDir - lightDir);
        float3 specular = pow(max(dot(normal, halfDir), 0.0), 32.0) * spotLight.specular;

        light += unshadowed * intensity * (diffuse + specular);
    }

    for (uint i = 0; i < pcs.pointLightCount; i++)
    {
        PointLightData pointLight = pcs.pointLights[i];

        float3 normal = vertData.normal;
        float3 lightVec = vertData.worldPos.xyz - pointLight.position;
        float3 offsetPos = vertData.worldPos.xyz + (vertData.normal * 8) - pointLight.position;
        float3 offsetLightDir = normalize(offsetPos);

        float sampleDepth = length(offsetPos) / pointLight.maxRange;

        TextureCube cubemap = textures[pointLight.shadowID];

        float unshadowed = cubemap.SampleCmp(shadowSampler, offsetLightDir, sampleDepth);

        float distance = length(lightVec);
        float attenuation = 1.0 / (pointLight.constant + (pointLight.linear * distance) + (pointLight.quadratic * distance * distance));

        float3 lightDir = normalize(lightVec);

        float3 diffuse = max(dot(normal, -lightDir), 0.0) * pointLight.diffuse;
        float3 viewDir = normalize(-vertData.eyeRelPos);
        float3 halfDir = normalize(viewDir - lightDir);
        float3 specular = pow(max(dot(normal, halfDir), 0.0), 32.0) * pointLight.specular;

        light += unshadowed * attenuation * (diffuse + specular);
    }

    return color * float4(light, 1.0);
}