#version 450

layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D uAtlas;

layout(push_constant) uniform PC
{
    vec4 params; // x=pxRange, y=debug(0/1)
} pc;

float median3(float a, float b, float c)
{
    return max(min(a,b), min(max(a,b), c));
}

void main()
{
    vec3 s = texture(uAtlas, vUv).rgb;

    if (pc.params.y > 0.5)
    {
        outColor = vec4(s, 1.0);
        return;
    }

    float sd = median3(s.r, s.g, s.b) - 0.5;

    vec2 texSize = vec2(textureSize(uAtlas, 0));
    vec2 unitRange = vec2(pc.params.x) / texSize;
    vec2 screenTexSize = vec2(1.0) / fwidth(vUv);
    float screenPxRange = max(0.5 * dot(unitRange, screenTexSize), 1.0);

    float alpha = clamp(sd * screenPxRange + 0.5, 0.0, 1.0);

    // белый текст
    outColor = vec4(1.0, 1.0, 1.0, alpha);
}
