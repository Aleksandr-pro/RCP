#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec2 inUV;

layout(location = 0) out vec3 vertexColor;
layout(location = 1) out vec2 vertexUV;

layout(push_constant) uniform PushConstants
{
    mat4 mvp;
    vec4 lightDirection;
} pushConstants;

void main()
{
    gl_Position = pushConstants.mvp * vec4(inPosition, 1.0);

    vec3 normal = normalize(inNormal);
    vec3 light = normalize(pushConstants.lightDirection.xyz);

    float diffuse = max(dot(normal, light), 0.0);
    float brightness = 0.25 + 0.75 * diffuse;

    vertexColor = inColor * brightness;
    vertexUV = inUV;
}
