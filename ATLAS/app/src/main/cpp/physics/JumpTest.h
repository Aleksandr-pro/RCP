#pragma once

#include "VerticalPhysics.h"

#include <cmath>
#include <cstddef>

namespace atlas::physics
{
    // Состояние одноразового тестового прыжка.
    struct JumpTest
    {
        float groundedTime = 0.0f;
        bool jumped = false;
    };

    // Вызывать один раз перед каждым фиксированным шагом физики.
    //
    // Тестовый куб должен быть отдельно от стопки:
    // его проекция X/Z не должна пересекать другие тела.
    //
    // Возвращает false при недопустимых аргументах.
    inline bool updateJumpTest(
        JumpTest& test,
        const Platform& platform,
        std::vector<Box>& boxes,
        std::size_t boxIndex,
        float dt
    )
    {
        constexpr float WAIT_SECONDS = 1.0f;
        constexpr float JUMP_SPEED = 4.0f;
        constexpr float CONTACT_EPSILON = 0.001f;

        if (boxIndex >= boxes.size() ||
            !std::isfinite(dt) ||
            dt <= 0.0f ||
            dt > 0.02f)
        {
            return false;
        }

        if (test.jumped)
        {
            return true;
        }

        Box& box = boxes[boxIndex];

        const float platformTop =
            platform.y + platform.halfY;

        const float boxBottom =
            box.y - box.halfY;

        const bool overlapsX =
            std::abs(box.x - platform.x) <
            box.halfX + platform.halfX;

        const bool overlapsZ =
            std::abs(box.z - platform.z) <
            box.halfZ + platform.halfZ;

        const bool onPlatform =
            overlapsX &&
            overlapsZ &&
            std::abs(boxBottom - platformTop) <= CONTACT_EPSILON &&
            std::abs(box.velocityY) <= CONTACT_EPSILON;

        if (!onPlatform)
        {
            test.groundedTime = 0.0f;
            return true;
        }

        test.groundedTime += dt;

        if (test.groundedTime >= WAIT_SECONDS)
        {
            box.velocityY = JUMP_SPEED;
            test.jumped = true;
        }

        return true;
    }
}
