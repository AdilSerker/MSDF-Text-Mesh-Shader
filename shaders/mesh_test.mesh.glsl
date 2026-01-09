#version 460
#extension GL_EXT_mesh_shader : require

layout(local_size_x = 1) in;
layout(triangles) out;
layout(max_vertices = 4, max_primitives = 2) out;

struct GlyphInstance
{
    vec2 posMin; // NDC: (left, bottom)
    vec2 posMax; // NDC: (right, top)
    vec2 uvMin;  // (u0, vTop)   - v=0 вверху
    vec2 uvMax;  // (u1, vBottom)
};

layout(set = 0, binding = 1, std430) readonly buffer Instances
{
    GlyphInstance inst[];
};

layout(location = 0) out vec2 vUv[];

void main()
{
    uint id = gl_WorkGroupID.x;
    GlyphInstance g = inst[id];

    SetMeshOutputsEXT(4, 2);

    // positions
    gl_MeshVerticesEXT[0].gl_Position = vec4(g.posMin.x, g.posMin.y, 0.0, 1.0); // BL
    gl_MeshVerticesEXT[1].gl_Position = vec4(g.posMax.x, g.posMin.y, 0.0, 1.0); // BR
    gl_MeshVerticesEXT[2].gl_Position = vec4(g.posMax.x, g.posMax.y, 0.0, 1.0); // TR
    gl_MeshVerticesEXT[3].gl_Position = vec4(g.posMin.x, g.posMax.y, 0.0, 1.0); // TL

    // uvs (v=0 top)
    float u0 = g.uvMin.x;
    float u1 = g.uvMax.x;
    float vT = g.uvMin.y;
    float vB = g.uvMax.y;

    vUv[0] = vec2(u0, vB); // BL
    vUv[1] = vec2(u1, vB); // BR
    vUv[2] = vec2(u1, vT); // TR
    vUv[3] = vec2(u0, vT); // TL

    gl_PrimitiveTriangleIndicesEXT[0] = uvec3(0, 1, 2);
    gl_PrimitiveTriangleIndicesEXT[1] = uvec3(0, 2, 3);
}
