#pragma once

#include <vector>

namespace atlas::physics
{
    struct Box
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;

        // Половина размера по каждой оси.
        float halfX = 0.5f;
        float halfY = 0.5f;
        float halfZ = 0.5f;

        float velocityY = 0.0f;
    };

    struct Platform
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;

        float halfX = 1.0f;
        float halfY = 0.1f;
        float halfZ = 1.0f;
    };

    // Проверка начального состояния.
    bool validate(
        const Platform& platform,
        const std::vector<Box>& boxes
    );

    // Один фиксированный шаг.
    // Возвращает false при недопустимых параметрах.
    bool step(
        const Platform& platform,
        std::vector<Box>& boxes,
        float dt
    );
}
