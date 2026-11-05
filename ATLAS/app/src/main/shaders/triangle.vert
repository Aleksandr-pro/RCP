#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 vertexColor;

layout(push_constant) uniform PushConstants
{
    float aspectRatio;
    float rotation;
} pushConstants;

void main()
{
    vec3 position = inPosition;

    float angleY = 0.55;
    float cosY = cos(angleY);
    float sinY = sin(angleY);

    vec3 rotatedY = vec3(
        cosY * position.x + sinY * position.z,
        position.y,
        -sinY * position.x + cosY * position.z
    );

    float angleX = -0.35;
    float cosX = cos(angleX);
    float sinX = sin(angleX);

    vec3 rotated = vec3(
        rotatedY.x,
        cosX * rotatedY.y - sinX * rotatedY.z,
        sinX * rotatedY.y + cosX * rotatedY.z
    );

    float distanceToCamera = 3.0 + rotated.z;

    float aspect = max(pushConstants.aspectRatio, 0.01);
    float scale = min(aspect, 1.0) * 1.8;

    const float nearPlane = 0.1;
    const float farPlane = 20.0;

    float clipZ =
        farPlane / (farPlane - nearPlane) * distanceToCamera -
        (farPlane * nearPlane) / (farPlane - nearPlane);

    // Координаты для изображения в итоговой ориентации экрана.
    vec2 screenXY = vec2(
        rotated.x * scale / aspect,
        rotated.y * scale
    );

    // Ориентацию поверхности для этого теста обрабатывает система.
    vec2 bufferXY = screenXY;

    gl_Position = vec4(
        bufferXY,
        clipZ,
        distanceToCamera
    );

    vertexColor = inColor;
}
