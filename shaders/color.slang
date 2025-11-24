// Represents transformation data for the camera
struct CameraData
{
    float4x4 view;
    float4x4 proj;
    float3 pos;
};

// Represents transformation and material data for the objects in the scene
struct ObjectData
{
    float4x4 model;
    int texID;
    float4 color;
};

struct DirLightData
{
    float3 direction;
    int shadowID;

    float3 diffuse;
    float3 specular;
};

struct SpotLightData
{
    float4x4 lightSpace;

    float3 position;
    float3 direction;
    int shadowID;

    float3 diffuse;
    float3 specular;

    float innerCutoff;
    float outerCutoff;
    float range;
};

struct PointLightData
{
    float3 position;
    int shadowID;

    float3 diffuse;
    float3 specular;

    float constant;
    float linear;
    float quadratic;

    float maxRange;
};

struct LightCascade
{
    float4x4 lightSpace;
    float maxDepth;
}

// Represents a single vertex of a mesh
struct Vertex
{
    float3 pos;
    float uvX;
    float3 normal;
    float uvY;
};

struct PushConstants
{
    CameraData *camera;
    ObjectData *objects;
    Vertex *vertices;
    DirLightData *dirLights;
    LightCascade *dirCascades;
    SpotLightData *spotLights;
    PointLightData *pointLights;
    uint dirLightCount;
    uint dirCascadeCount;
    uint spotLightCount;
    uint pointLightCount;
    float3 ambientLight;
}


// Represents a vertex that has been processed by the vertex shader
struct CoarseVertex
{
    float4 worldPos;
    float3 normal;
    float uvX;
    float3 eyeRelPos;
    float uvY;
    int instance;
};

// Represents a processed vertex along with its screenspace position
struct VertexOutput
{
    float4 pos : SV_Position;
    CoarseVertex vertData : CoarseVertex;
};


[vk::push_constant]
PushConstants pcs;

[shader("vertex")]
VertexOutput vertexMain(uint vertexID : SV_VertexID,
    uint instanceID : SV_InstanceID, uint baseInstance : SV_StartInstanceLocation)
{
    ObjectData object = pcs.objects[baseInstance + instanceID];

    float4x4 model = object.model;
    Vertex vert = pcs.vertices[vertexID];
    float4 pos = float4(vert.pos, 1.0);
    float4 worldPos = mul(pos, model);

    VertexOutput output;
    float3x3 normMat = float3x3(
                cross(model[1].xyz, model[2].xyz),
                cross(model[2].xyz, model[0].xyz),
                cross(model[0].xyz, model[1].xyz));

    float3 normal = normalize(mul(vert.normal, normMat));

    CoarseVertex vertData;
    vertData.eyeRelPos = worldPos.xyz - pcs.camera->pos;
    vertData.worldPos = worldPos;
    vertData.normal = normal;
    vertData.uvX = vert.uvX;
    vertData.uvY = vert.uvY;
    vertData.instance = baseInstance + instanceID;

    output.pos = mul(mul(worldPos, pcs.camera->view), pcs.camera->proj);
    output.vertData = vertData;

    return output;
};


[[vk::binding(0,0)]]
__DynamicResource<__DynamicResourceKind.General> textures[];

[[vk::binding(1,0)]]
SamplerComparisonState shadowSampler;

[[vk::binding(2,0)]]
SamplerState textureSampler;

[shader("fragment")]
float4 fragmentMain(CoarseVertex vertData : CoarseVertex) : SV_Target
{
    float4 viewPos = mul(vertData.worldPos, pcs.camera->view);

    float3 light = pcs.ambientLight;

    ObjectData object = pcs.objects[vertData.instance];

    float4 color = object.color;

    if (object.texID != -1)
    {
        Texture2D texture = textures[object.texID];
        color *= texture.Sample(textureSampler, float2(vertData.uvX, vertData.uvY));
    }

    for (uint i = 0; i < pcs.dirLightCount; i++)
    {
        DirLightData dirLight = pcs.dirLights[i];

        uint cascadeCount = pcs.dirCascadeCount;
        uint firstCascade = i * cascadeCount;
        uint lastCascade = ((i + 1) * cascadeCount);

        int cascade = -1;
        for (uint c = firstCascade; c < lastCascade; c++)
        {
            if (viewPos.z < pcs.dirCascades[c].maxDepth)
            {
                cascade = c;
                break;
            }
        }
        if (cascade == -1)
        {
            cascade = lastCascade;
        }

        float4 lightRelPos = mul(vertData.worldPos + float4(vertData.normal * 8, 0.0), pcs.dirCascades[cascade].lightSpace);

        float3 normal = vertData.normal;

        float3 lightPosNorm = (lightRelPos.xyz / lightRelPos.w);
        float3 lightPosScaled = float3(lightPosNorm.xy * 0.5 + 0.5, lightPosNorm.z);

        Texture2DArray cascadedMap = textures[dirLight.shadowID];

        float unshadowed = 0;

        for (int x = -1; x <= 1; x++)
        {
            for (int y = -1; y <= 1; y++)
            {
                unshadowed += cascadedMap.SampleCmp(shadowSampler, float3(lightPosScaled.xy, cascade), lightPosScaled.z, int2(x, y)) / 9;
            }
        }

        float3 diffuse = max(dot(normal, -dirLight.direction), 0.0) * dirLight.diffuse;
        float3 viewDir = normalize(-vertData.eyeRelPos);
        float3 halfDir = normalize(viewDir - dirLight.direction);
        float3 specular = pow(max(dot(normal, halfDir), 0.0), 32.0) * dirLight.specular;

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
};