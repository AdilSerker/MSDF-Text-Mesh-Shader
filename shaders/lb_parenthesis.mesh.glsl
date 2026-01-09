#version 460
#extension GL_EXT_mesh_shader : require

layout(local_size_x = 32, local_size_y = 1, local_size_z = 1) in;

// 10 примитивов * 3 вершины = 30 вершин
layout(triangles, max_vertices = 64, max_primitives = 32) out;

// per-vertex varyings
layout(location = 0) out vec2 vUV[];
layout(location = 1) out flat uint vPrimType[]; // <-- ВМЕСТО perprimitiveEXT

const uint SOLID   = 0u;
const uint CONVEX  = 1u;
const uint CONCAVE = 2u;

// Данные глифа ")" (как в статье)
const vec2 pos[10] = vec2[](
    vec2(+0.000, -1.00), // 0
    vec2(+0.150, -0.50), // 1
    vec2(+0.150, +0.00), // 2
    vec2(+0.150, +0.50), // 3
    vec2(+0.000, +1.00), // 4
    vec2(-0.300, +1.00), // 5
    vec2(-0.165, +0.50), // 6
    vec2(-0.165, +0.00), // 7
    vec2(-0.165, -0.50), // 8
    vec2(-0.300, -1.00)  // 9
);

const uvec3 tri[10] = uvec3[](
    uvec3(0, 1, 2), // 0  CONVEX
    uvec3(2, 3, 4), // 1  CONVEX
    uvec3(5, 6, 7), // 2  CONCAVE
    uvec3(7, 8, 9), // 3  CONCAVE
    uvec3(4, 5, 6), // 4  SOLID
    uvec3(4, 6, 2), // 5  SOLID
    uvec3(6, 7, 2), // 6  SOLID
    uvec3(7, 8, 2), // 7  SOLID
    uvec3(2, 8, 0), // 8  SOLID
    uvec3(9, 0, 8)  // 9  SOLID
);

const uint primAttr[10] = uint[](
    CONVEX,
    CONVEX,
    CONCAVE,
    CONCAVE,
    SOLID,
    SOLID,
    SOLID,
    SOLID,
    SOLID,
    SOLID
);

vec4 toNDC(vec2 p)
{
    float s = 0.45;
    vec2 t = vec2(-0.2, 0.0);
    vec2 ndc = p * s + t;
    return vec4(ndc, 0.0, 1.0);
}

void main()
{
    uint tid = gl_LocalInvocationID.x;

    const uint nPrims = 10u;
    const uint nVerts = nPrims * 3u;

    if (tid == 0u) {
        SetMeshOutputsEXT(nVerts, nPrims);
    }
    barrier();

    if (tid < nPrims)
    {
        uvec3 idx = tri[tid];
        uint base = tid * 3u;
        uint ttype = primAttr[tid];

        gl_MeshVerticesEXT[base + 0u].gl_Position = toNDC(pos[idx.x]);
        vUV[base + 0u] = vec2(0.0, 0.0);
        vPrimType[base + 0u] = ttype;

        gl_MeshVerticesEXT[base + 1u].gl_Position = toNDC(pos[idx.y]);
        vUV[base + 1u] = vec2(0.5, 0.0);
        vPrimType[base + 1u] = ttype;

        gl_MeshVerticesEXT[base + 2u].gl_Position = toNDC(pos[idx.z]);
        vUV[base + 2u] = vec2(1.0, 1.0);
        vPrimType[base + 2u] = ttype;

        gl_PrimitiveTriangleIndicesEXT[tid] = uvec3(base + 0u, base + 1u, base + 2u);
    }
}
