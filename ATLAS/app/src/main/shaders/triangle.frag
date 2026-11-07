#version 450

layout(location = 0) in vec3 vertexColor;
layout(location = 1) in vec2 vertexUV;

layout(location = 0) out vec4 outColor;

void main()
{
    vec2 cell = floor(vertexUV * 8.0);
    float checker = mod(cell.x + cell.y, 2.0);

    float pattern = mix(1.0, 0.35, checker);
    outColor = vec4(vertexColor * pattern, 1.0);
}
