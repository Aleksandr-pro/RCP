#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 vertexColor;

layout(push_constant) uniform PushConstants
{
    float aspectRatio;

    // Оставляем второе поле для совместимости с текущим C++-кодом.
    // В этой версии оно не используется.
    float reserved;
} pushConstants;

// Матрица поворота вокруг оси Y.
mat4 rotationY(float angle)
{
    float c = cos(angle);
    float s = sin(angle);

    // В GLSL аргументы mat4 задаются по столбцам.
    return mat4(
         c,   0.0, -s,  0.0,
         0.0, 1.0, 0.0, 0.0,
         s,   0.0,  c,  0.0,
         0.0, 0.0, 0.0, 1.0
    );
}

// Матрица поворота вокруг оси X.
mat4 rotationX(float angle)
{
    float c = cos(angle);
    float s = sin(angle);

    return mat4(
        1.0, 0.0, 0.0, 0.0,
        0.0, c,   s,   0.0,
        0.0, -s,  c,   0.0,
        0.0, 0.0, 0.0, 1.0
    );
}

// Камера находится условно перед объектом.
// В этой тестовой системе положительная Z направлена вглубь сцены.
mat4 cameraView()
{
    return mat4(
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 3.0, 1.0
    );
}

// Перспектива с диапазоном глубины Vulkan от 0 до 1.
mat4 perspective(float aspect)
{
    const float nearPlane = 0.1;
    const float farPlane = 20.0;

    // Сохраняем прежний приблизительный размер фигуры.
    float scale = min(aspect, 1.0) * 1.8;

    float depthScale = farPlane / (farPlane - nearPlane);
    float depthOffset =
        -(farPlane * nearPlane) / (farPlane - nearPlane);

    return mat4(
        scale / aspect, 0.0,   0.0,        0.0,
        0.0,            scale, 0.0,        0.0,
        0.0,            0.0,   depthScale, 1.0,
        0.0,            0.0,   depthOffset, 0.0
    );
}

void main()
{
    float aspect = max(pushConstants.aspectRatio, 0.01);

    mat4 model = rotationX(-0.35) * rotationY(0.55);
    mat4 view = cameraView();
    mat4 projection = perspective(aspect);

    gl_Position =
        projection * view * model * vec4(inPosition, 1.0);

    vertexColor = inColor;
}
