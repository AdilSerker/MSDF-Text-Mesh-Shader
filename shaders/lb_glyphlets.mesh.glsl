#version 460
#extension GL_EXT_mesh_shader : require

layout(local_size_x = 32) in;

layout(triangles, max_vertices = 64, max_primitives = 32) out;

// per-vertex varyings
layout(location = 0) out vec2 vUV[];
layout(location = 1) out flat uint vPrimType[];

// Buffers
layout(set = 0, binding = 0, std430) readonly buffer PositionsBuf { vec2 pos[]; } positions;
layout(set = 0, binding = 1, std430) readonly buffer IndicesBuf   { uvec3 tri[]; } indices;
layout(set = 0, binding = 2, std430) readonly buffer PrimTypeBuf  { uint primType[]; } ptypes;
layout(set = 0, binding = 3, std430) readonly buffer InstancesBuf { vec2 offsetNDC[]; } inst;

const uint nPrims = 10u;
const uint nVerts = nPrims * 3u;

vec4 toNDC(vec2 p, vec2 instOff)
{
    // Та же логика, что была в прототипе — только теперь instOff приходит из SSBO.
    float s = 0.45;
    vec2 t = vec2(-0.2, 0.0);
    vec2 ndc = p * s + t + instOff;
    return vec4(ndc, 0.0, 1.0);
}

void main()
{
    uint primID = gl_LocalInvocationID.x;
    uint glyphInstance = gl_WorkGroupID.x;

    if (primID == 0u) {
        SetMeshOutputsEXT(nVerts, nPrims);
    }
    barrier();

    if (primID < nPrims)
    {
        uvec3 idx = indices.tri[primID];
        uint base = primID * 3u;
        uint ttype = ptypes.primType[primID];
        vec2 off = inst.offsetNDC[glyphInstance];

        gl_MeshVerticesEXT[base + 0u].gl_Position = toNDC(positions.pos[idx.x], off);
        vUV[base + 0u] = vec2(0.0, 0.0);
        vPrimType[base + 0u] = ttype;

        gl_MeshVerticesEXT[base + 1u].gl_Position = toNDC(positions.pos[idx.y], off);
        vUV[base + 1u] = vec2(0.5, 0.0);
        vPrimType[base + 1u] = ttype;

        gl_MeshVerticesEXT[base + 2u].gl_Position = toNDC(positions.pos[idx.z], off);
        vUV[base + 2u] = vec2(1.0, 1.0);
        vPrimType[base + 2u] = ttype;

        gl_PrimitiveTriangleIndicesEXT[primID] = uvec3(base + 0u, base + 1u, base + 2u);
    }
}
