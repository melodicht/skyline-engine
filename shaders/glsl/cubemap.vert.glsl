#version 460

#extension GL_EXT_scalar_block_layout: require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_multiview : require

struct CameraData
{
    mat4 view;
    mat4 proj;
    vec3 pos;
};


layout (buffer_reference, scalar) readonly buffer CameraBuffer
{
    CameraData cameras[];
};

struct ObjectData
{
    mat4 model;
    int texID;
    mat4 color;
};

layout (buffer_reference, scalar) readonly buffer ObjectBuffer
{
    ObjectData objects[];
};

struct Vertex
{
    vec3 pos;
    float uvX;
    vec3 normal;
    float uvY;
};

layout (buffer_reference, scalar) readonly buffer VertexBuffer
{
    Vertex vertices[];
};

layout (push_constant, scalar) uniform PushConstants
{
    CameraBuffer cameraBuffer;
    VertexBuffer vertexBuffer;
    ObjectBuffer objectBuffer;
} pcs;

layout(location = 0) out vec3 outWorldPos;

void main()
{
    ObjectData object = pcs.objectBuffer.objects[gl_BaseInstance + gl_InstanceIndex];

    mat4 model = object.model;
    Vertex vert = pcs.vertexBuffer.vertices[gl_VertexIndex];
    vec4 pos = vec4(vert.pos, 1.0);
    vec4 worldPos = model * pos;

    CameraData camera = pcs.cameraBuffer.cameras[gl_ViewIndex];

    gl_Position = camera.proj * camera.view * worldPos;
    outWorldPos = worldPos.xyz;
}