#version 460

vec4 vertices[3] =
{
    vec4(0, 0, 0, 1),
    vec4(0, 2, 0, 1),
    vec4(2, 0, 0, 1),
};

void main()
{
    gl_Position = vertices[gl_VertexIndex];
}