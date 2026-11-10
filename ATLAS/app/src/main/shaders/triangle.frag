#version 450

layout(location = 0) in float vertexBrightness;
layout(location = 1) in vec2 vertexUV;
layout(location = 2) in float terrainHeight;
layout(location = 3) flat in float isTerrain;
layout(location = 4) in vec2 worldXZ;
layout(set = 0, binding = 0) uniform sampler2D imageTexture;
layout(set = 0, binding = 1) uniform sampler2D sandTexture;
layout(set = 0, binding = 2) uniform sampler2D groundTexture;
layout(set = 0, binding = 3) uniform sampler2D stoneTexture;
layout(location = 0) out vec4 outColor;

// World-space variation: the same coordinates produce the same pattern on both sides of a chunk boundary.
float terrainVariation(vec2 worldXZ)
{
    float a = sin(worldXZ.x * 0.13 + worldXZ.y * 0.07);
    float b = cos(worldXZ.x * 0.05 - worldXZ.y * 0.11);
    return clamp(0.5 + 0.25 * a * b, 0.0, 1.0);
}

vec3 variedTexture(sampler2D tex, vec2 worldXZ, float variation)
{
    vec2 uvA = worldXZ * 0.25;
    vec2 uvB = worldXZ * 0.17 + vec2(0.37, 0.61);
    vec3 a = texture(tex, uvA).rgb;
    vec3 b = texture(tex, uvB).rgb;
    return mix(a, b, variation);
}

void main()
{
    vec3 surfaceColor;
    if (isTerrain > 0.5)
    {
        float sandToGround = smoothstep(-0.30, -0.10, terrainHeight);
        float groundToStone = smoothstep(0.25, 0.45, terrainHeight);
        float sandWeight = 1.0 - sandToGround;
        float stoneWeight = groundToStone;
        float groundWeight = sandToGround - groundToStone;
        float variation = terrainVariation(worldXZ);
        vec3 sandColor = variedTexture(sandTexture, worldXZ, variation);
        vec3 groundColor = variedTexture(groundTexture, worldXZ, variation);
        vec3 stoneColor = variedTexture(stoneTexture, worldXZ, variation);
        surfaceColor = sandColor * sandWeight + groundColor * groundWeight + stoneColor * stoneWeight;
    }
    else
        surfaceColor = texture(imageTexture, vertexUV).rgb;
    outColor = vec4(surfaceColor * vertexBrightness, 1.0);
}
