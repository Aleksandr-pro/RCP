#pragma once

#include "SurfaceBounce.h"

#include <algorithm>
#include <cmath>

namespace atlas::physics
{
    struct SlideMotion
    {
        float velocityX = 0.0f;
        float velocityZ = 0.0f;
    };

    // Упрощённое трение для отдельного куба на неподвижной платформе.
    //
    // Ограничения:
    // - куб не взаимодействует с другими телами;
    // - куб полностью находится над платформой;
    // - вращение и боковые столкновения не моделируются;
    // - трение действует только когда куб покоится по Y на платформе.
    //
    // При ошибке box и motion остаются без изменений.
    inline bool stepSlidingBox(
        const Platform& platform,
        Box& box,
        SlideMotion& motion,
        const ContactMaterial& contact,
        float friction,
        float dt
    )
    {
        constexpr float GRAVITY_MAGNITUDE = 9.81f;
        constexpr float CONTACT_EPSILON = 0.0001f;

        if (!std::isfinite(dt) || dt <= 0.0f || dt > 0.02f ||
            !std::isfinite(friction) || friction < 0.0f ||
            !std::isfinite(motion.velocityX) ||
            !std::isfinite(motion.velocityZ) ||
            !std::isfinite(box.x) ||
            !std::isfinite(box.z) ||
            !std::isfinite(box.halfX) ||
            !std::isfinite(box.halfZ) ||
            box.halfX <= 0.0f ||
            box.halfZ <= 0.0f ||
            !std::isfinite(platform.x) ||
            !std::isfinite(platform.z) ||
            !std::isfinite(platform.halfX) ||
            !std::isfinite(platform.halfZ) ||
            platform.halfX <= 0.0f ||
            platform.halfZ <= 0.0f)
        {
            return false;
        }

        // Работаем с копиями: при ошибке не изменяем исходное состояние.
        Box nextBox = box;
        SlideMotion nextMotion = motion;

        if (!stepBouncingBox(platform, nextBox, contact, dt))
        {
            return false;
        }

        const float platformTop = platform.y + platform.halfY;
        const float bottom = nextBox.y - nextBox.halfY;

        const bool grounded =
            std::abs(bottom - platformTop) <= CONTACT_EPSILON &&
            nextBox.velocityY == 0.0f;

        if (grounded)
        {
            const float speed = std::hypot(
                nextMotion.velocityX,
                nextMotion.velocityZ
            );

            if (!std::isfinite(speed))
            {
                return false;
            }

            if (speed > 0.0f)
            {
                // Приближение сухого трения: a = μg.
                const float newSpeed = std::max(
                    0.0f,
                    speed - friction * GRAVITY_MAGNITUDE * dt
                );

                const float factor = newSpeed / speed;
                nextMotion.velocityX *= factor;
                nextMotion.velocityZ *= factor;
            }
        }

        nextBox.x += nextMotion.velocityX * dt;
        nextBox.z += nextMotion.velocityZ * dt;

        if (!std::isfinite(nextBox.x) || !std::isfinite(nextBox.z))
        {
            return false;
        }

        // Пока край платформы не моделируется: если куб выходит
        // за её пределы, останавливаем тест с ошибкой.
        const float minX = platform.x - platform.halfX;
        const float maxX = platform.x + platform.halfX;
        const float minZ = platform.z - platform.halfZ;
        const float maxZ = platform.z + platform.halfZ;

        if (!std::isfinite(minX) || !std::isfinite(maxX) ||
            !std::isfinite(minZ) || !std::isfinite(maxZ) ||
            nextBox.x - nextBox.halfX < minX ||
            nextBox.x + nextBox.halfX > maxX ||
            nextBox.z - nextBox.halfZ < minZ ||
            nextBox.z + nextBox.halfZ > maxZ)
        {
            return false;
        }

        box = nextBox;
        motion = nextMotion;
        return true;
    }
}
