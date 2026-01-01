#version 460

#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_nonuniform_qualifier : require

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
} pcs;

layout(location = 0) in vec2 uv;
layout(location = 1) flat in int instance;

layout(location = 0) out vec4 outFragColor;
layout(location = 1) out uint outID;

layout(set = 0, binding = 0) uniform texture2D textures[];

layout(set = 0, binding = 2) uniform sampler textureSampler;

void main()
{
    ObjectData object = pcs.objectBuffer.objects[instance];
    vec4 color = texture(sampler2D(textures[nonuniformEXT(object.texID)], textureSampler), uv);
    if (color.a < 0.5)
    {
        discard;
    }
    outID = object.id;

    outFragColor = color;
}