#version 450

layout(location = 0) in float vertexBrightness;
layout(location = 1) in vec2 vertexUV;
layout(set = 0, binding = 0) uniform sampler2D imageTexture;
layout(location = 0) out vec4 outColor;

void main()
{
    outColor = vec4(texture(imageTexture, vertexUV).rgb * vertexBrightness, 1.0);
}
