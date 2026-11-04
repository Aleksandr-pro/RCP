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
    // Временное преобразование тестовых вершин из версии 0.6b.
    float depthOffset = inPosition.z - 0.3;

    vec3 position;
    position.x = inPosition.x - 0.5 * depthOffset + 0.15;
    position.y = inPosition.y - 0.5 * depthOffset + 0.05;
    position.z = (inPosition.z - 0.5) * 2.5;

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

    // Поворот изображения в соответствии с трансформацией поверхности.
    vec2 bufferXY = screenXY;

    if (pushConstants.rotation > 0.5 &&
        pushConstants.rotation < 1.5)
    {
        bufferXY = vec2(-screenXY.y, screenXY.x);
    }
    else if (pushConstants.rotation > 1.5 &&
             pushConstants.rotation < 2.5)
    {
        bufferXY = -screenXY;
    }
    else if (pushConstants.rotation > 2.5)
    {
        bufferXY = vec2(screenXY.y, -screenXY.x);
    }

    gl_Position = vec4(
        bufferXY,
        clipZ,
        distanceToCamera
    );

    vertexColor = inColor;
}
