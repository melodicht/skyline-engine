#version 460

#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_nonuniform_qualifier : require

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
    vec4 color;
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

layout (buffer_reference, scalar) readonly buffer DirLightBuffer
{
    DirLightData lights[];
};

layout (buffer_reference, scalar) readonly buffer SpotLightBuffer
{
    SpotLightData lights[];
};

layout (buffer_reference, scalar) readonly buffer PointLightBuffer
{
    PointLightData lights[];
};

layout (buffer_reference, scalar) readonly buffer LightCascadeBuffer
{
    LightCascade cascades[];
};

layout (buffer_reference, scalar) readonly buffer IdBuffer
{
    uint ids[];
};

layout (push_constant, scalar) uniform PushConstants
{
    CameraBuffer cameraBuffer;
    ObjectBuffer objectBuffer;
    layout (offset = 24) IdBuffer idBuffer;
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
layout(location = 5) flat in int instance;

layout(location = 0) out vec4 outFragColor;
layout(location = 1) out uint outID;

layout(set = 0, binding = 0) uniform texture2D textures[];
layout(set = 0, binding = 0) uniform texture2DArray arrayTextures[];
layout(set = 0, binding = 0) uniform textureCube cubemaps[];

layout(set = 0, binding = 1) uniform samplerShadow shadowSampler;

layout(set = 0, binding = 2) uniform sampler textureSampler;

void main()
{
    outID = pcs.idBuffer.ids[instance];
    vec4 viewPos = pcs.cameraBuffer.camera.view * worldPos;

    vec3 light = pcs.ambientLight;

    ObjectData object = pcs.objectBuffer.objects[instance];

    vec4 color = object.color;

    if (object.texID != -1)
    {
        color *= texture(sampler2D(textures[nonuniformEXT(object.texID)], textureSampler), vec2(uvX, uvY));
    }

    for (uint i = 0; i < pcs.dirLightCount; i++)
    {
        DirLightData dirLight = pcs.dirLightBuffer.lights[i];

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

        vec4 lightRelPos = pcs.dirCascadeBuffer.cascades[cascade].lightSpace * worldPos;

        vec3 lightPosNorm = (lightRelPos.xyz / lightRelPos.w);
        vec3 lightPosScaled = vec3(lightPosNorm.xy * 0.5 + 0.5, lightPosNorm.z);

        float unshadowed = 0;

        vec2 texelSize = 1.0 / textureSize(sampler2DArrayShadow(arrayTextures[nonuniformEXT(dirLight.shadowID)], shadowSampler), 0).xy;

        for (int x = -1; x <= 1; x++)
        {
            for (int y = -1; y <= 1; y++)
            {
                unshadowed += texture(sampler2DArrayShadow(arrayTextures[nonuniformEXT(dirLight.shadowID)], shadowSampler),
                    vec4(lightPosScaled.xy + (vec2(x, y) * texelSize), cascade, lightPosScaled.z)) / 9;
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
        SpotLightData spotLight = pcs.spotLightBuffer.lights[i];

        vec4 lightRelPos = spotLight.lightSpace * worldPos;
        vec3 offsetPos = worldPos.xyz - spotLight.position;

        vec3 lightPosNorm = (lightRelPos.xyz / lightRelPos.w);
        float sampleDepth = length(offsetPos) / spotLight.range;
        vec3 lightPosScaled = vec3(lightPosNorm.xy * 0.5 + 0.5, sampleDepth);

        vec2 texelSize = 1.0 / textureSize(sampler2DShadow(textures[nonuniformEXT(spotLight.shadowID)], shadowSampler), 0).xy;

        float unshadowed = 0;

        for (int x = -1; x <= 1; x++)
        {
            for (int y = -1; y <= 1; y++)
            {
                unshadowed += texture(sampler2DShadow(textures[nonuniformEXT(spotLight.shadowID)], shadowSampler),
                    vec3(lightPosScaled.xy + (vec2(x, y) * texelSize), lightPosScaled.z)) / 9;
            }
        }

        vec3 lightDir = normalize(offsetPos);
        float lightAngle = dot(lightDir, spotLight.direction);

        float fadeSize = spotLight.innerCutoff - spotLight.outerCutoff;
        float intensity = clamp((lightAngle - spotLight.outerCutoff) / fadeSize, 0.0, 1.0);

        vec3 diffuse = max(dot(normal, -lightDir), 0.0) * spotLight.diffuse;
        vec3 viewDir = normalize(-eyeRelPos);
        vec3 halfDir = normalize(viewDir - lightDir);
        vec3 specular = pow(max(dot(normal, halfDir), 0.0), 32.0) * spotLight.specular;

        light += unshadowed * intensity * (diffuse + specular);
    }

    for (uint i = 0; i < pcs.pointLightCount; i++)
    {
        PointLightData pointLight = pcs.pointLightBuffer.lights[i];

        vec3 lightVec = worldPos.xyz - pointLight.position;
        vec3 offsetPos = worldPos.xyz - pointLight.position;
        vec3 offsetLightDir = normalize(offsetPos);

        float sampleDepth = length(offsetPos) / pointLight.maxRange;

        float unshadowed = texture(samplerCubeShadow(cubemaps[nonuniformEXT(pointLight.shadowID)], shadowSampler), vec4(offsetLightDir, sampleDepth));

        float distance = length(lightVec);
        float attenuation = 1.0 / (pointLight.constant + (pointLight.linear * distance) + (pointLight.quadratic * distance * distance));

        vec3 lightDir = normalize(lightVec);

        vec3 diffuse = max(dot(normal, -lightDir), 0.0) * pointLight.diffuse;
        vec3 viewDir = normalize(-eyeRelPos);
        vec3 halfDir = normalize(viewDir - lightDir);
        vec3 specular = pow(max(dot(normal, halfDir), 0.0), 32.0) * pointLight.specular;

        light += unshadowed * attenuation * (diffuse + specular);
    }

    outFragColor = color * vec4(light, 1.0);
}