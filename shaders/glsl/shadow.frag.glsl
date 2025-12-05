#version 460

void main()
{
    gl_FragDepth = gl_FragCoord.z + fwidth(gl_FragCoord.z);
}
