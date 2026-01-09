#version 460
#extension GL_EXT_mesh_shader : require

layout(location = 0) in vec2 inUV;
layout(location = 1) flat in uint inPrimType;

layout(location = 0) out vec4 outColor;

const uint SOLID   = 0u;
const uint CONVEX  = 1u;
const uint CONCAVE = 2u;

void main()
{
    float y = inUV.x * inUV.x - inUV.y;

    if ((inPrimType == CONVEX  && y > 0.0) ||
        (inPrimType == CONCAVE && y < 0.0))
        discard;

    outColor = vec4(1.0);
}
