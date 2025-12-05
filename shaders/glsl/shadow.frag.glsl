#version 460

#extension GL_EXT_scalar_block_layout : require

layout (push_constant, scalar) uniform PushConstants
{
    layout (offset = 24) vec3 lightPos;
    float farPlane;
} pcs;

layout(location = 0) in vec3 worldPos;

void main()
{
    float distance = length(worldPos - pcs.lightPos) / pcs.farPlane;
    gl_FragDepth = distance + fwidth(distance);
}