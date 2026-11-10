#version 450

layout(location = 0) in float vertexBrightness;
layout(location = 1) in vec2 vertexUV;
layout(location = 2) in float terrainHeight;
layout(location = 3) flat in float isTerrain;
layout(set = 0, binding = 0) uniform sampler2D imageTexture;
layout(set = 0, binding = 1) uniform sampler2D sandTexture;
layout(set = 0, binding = 2) uniform sampler2D groundTexture;
layout(set = 0, binding = 3) uniform sampler2D stoneTexture;
layout(location = 0) out vec4 outColor;

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
        surfaceColor = texture(sandTexture, vertexUV).rgb * sandWeight +
                       texture(groundTexture, vertexUV).rgb * groundWeight +
                       texture(stoneTexture, vertexUV).rgb * stoneWeight;
    }
    else
        surfaceColor = texture(imageTexture, vertexUV).rgb;
    outColor = vec4(surfaceColor * vertexBrightness, 1.0);
}
