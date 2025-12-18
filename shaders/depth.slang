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

// Represents a single vertex of a mesh
struct Vertex
{
    float3 pos;
    float uvX;
    float3 normal;
    float uvY;
};

// Represents the memory locations of the camera, object, and vertex buffers
struct PushConstants
{
    CameraData *camera;
    ObjectData *objects;
    Vertex *vertices;
};

[vk::push_constant]
PushConstants pcs;

[shader("vertex")]
float4 vertexMain(uint vertexID : SV_VertexID,
    uint instanceID : SV_InstanceID, uint baseInstance : SV_StartInstanceLocation,
    uint viewID : SV_ViewID) : SV_Position
{
    ObjectData object = pcs.objects[baseInstance + instanceID];

    float4x4 model = object.model;
    Vertex vert = pcs.vertices[vertexID];
    float4 pos = float4(vert.pos, 1.0);
    float4 worldPos = mul(pos, model);

    return mul(mul(worldPos, pcs.camera[viewID].view), pcs.camera[viewID].proj);
};