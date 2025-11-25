#version 460

#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference : require

struct CameraData
{
    mat4 view;
    mat4 proj;
    vec3 pos;
};


layout (buffer_reference, scalar) readonly buffer CameraBuffer
{
    CameraData camera;
};

struct ObjectData
{
    mat4 model;
    int texID;
    vec4 color;
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
    ObjectBuffer objectBuffer;
    VertexBuffer vertexBuffer;
} pcs;

layout(location = 0) out vec4 outWorldPos;
layout(location = 1) out vec3 normal;
layout(location = 2) out float uvX;
layout(location = 3) out vec3 eyeRelPos;
layout(location = 4) out float uvY;
layout(location = 5) flat out int instance;

void main()
{
    ObjectData object = pcs.objectBuffer.objects[gl_InstanceIndex];

    mat4 model = object.model;
    Vertex vert = pcs.vertexBuffer.vertices[gl_VertexIndex];
    vec4 pos = vec4(vert.pos, 1.0);
    vec4 worldPos = model * pos;

    mat3 normMat = mat3(
        cross(model[1].xyz, model[2].xyz),
        cross(model[2].xyz, model[0].xyz),
        cross(model[0].xyz, model[1].xyz));

    CameraData camera = pcs.cameraBuffer.camera;

    gl_Position = camera.proj * camera.view * worldPos;
    outWorldPos = worldPos;
    normal = normalize(normMat * vert.normal);
    uvX = vert.uvX;
    uvY = vert.uvY;
    eyeRelPos = worldPos.xyz - camera.pos;
    instance = gl_InstanceIndex;
}