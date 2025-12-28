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
    vec3 pos;
    uint texID;
    uint id;
};

layout (buffer_reference, scalar) readonly buffer ObjectBuffer
{
    ObjectData objects[];
};

layout (push_constant, scalar) uniform PushConstants
{
    ObjectBuffer objectBuffer;
    CameraBuffer cameraBuffer;
    vec2 iconSize;
} pcs;

layout(location = 0) out vec2 uv;
layout(location = 1) flat out int instance;

vec2 offsets[4] =
{
    vec2(1, 1),
    vec2(1, -1),
    vec2(-1, 1),
    vec2(-1, -1)
};

vec2 uvs[4] =
{
    vec2(1, 1),
    vec2(1, 0),
    vec2(0, 1),
    vec2(0, 0)
};

void main()
{
    ObjectData object = pcs.objectBuffer.objects[gl_InstanceIndex];

    CameraData camera = pcs.cameraBuffer.camera;
    vec4 centerPos = camera.proj * camera.view * vec4(object.pos, 1.0);

    uv = uvs[gl_VertexIndex];

    gl_Position = centerPos + vec4(offsets[gl_VertexIndex] * pcs.iconSize, 0, 0);
    instance = gl_InstanceIndex;
}