#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 vertexColor;

// Параметр передаётся из C++ при записи команд рисования.
layout(push_constant) uniform PushConstants
{
    float aspectRatio;
} pushConstants;

void main()
{
    // В предыдущей версии стороны были сдвинуты вручную,
    // чтобы имитировать объём. Убираем этот сдвиг.
    float depthOffset = inPosition.z - 0.3;

    vec3 position;
    position.x = inPosition.x - 0.5 * depthOffset + 0.15;
    position.y = inPosition.y - 0.5 * depthOffset + 0.05;

    // Исходная глубина была 0.4, а ширина и высота — 1.0.
    // Приводим её к сопоставимому размеру.
    position.z = (inPosition.z - 0.5) * 2.5;

    // Фиксированный поворот вокруг вертикальной оси.
    float angleY = 0.55;
    float cosY = cos(angleY);
    float sinY = sin(angleY);

    vec3 rotatedY = vec3(
        cosY * position.x + sinY * position.z,
        position.y,
        -sinY * position.x + cosY * position.z
    );

    // Небольшой наклон вокруг горизонтальной оси.
    float angleX = -0.35;
    float cosX = cos(angleX);
    float sinX = sin(angleX);

    vec3 rotated = vec3(
        rotatedY.x,
        cosX * rotatedY.y - sinX * rotatedY.z,
        sinX * rotatedY.y + cosX * rotatedY.z
    );

    // Условно размещаем объект перед камерой.
    float distanceToCamera = 3.0 + rotated.z;

    // Сохраняем пропорции и в вертикальной,
    // и в горизонтальной ориентации экрана.
    float aspect = max(pushConstants.aspectRatio, 0.01);
    float scale = min(aspect, 1.0) * 1.8;

    // После деления на W получаем перспективу.
    // Глубина переводится в диапазон Vulkan: от 0 до 1.
    const float nearPlane = 0.1;
    const float farPlane = 20.0;

    float clipZ =
        farPlane / (farPlane - nearPlane) * distanceToCamera -
        (farPlane * nearPlane) / (farPlane - nearPlane);

    gl_Position = vec4(
        rotated.x * scale / aspect,
        rotated.y * scale,
        clipZ,
        distanceToCamera
    );

    vertexColor = inColor;
}
