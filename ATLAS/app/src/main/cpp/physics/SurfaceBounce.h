#pragma once

#include "VerticalPhysics.h"

#include <algorithm>
#include <cmath>

namespace atlas::physics
{
    // Параметры контакта конкретного тела и поверхности.
    // Это пока тестовые значения, не измеренные свойства материалов.
    struct ContactMaterial
    {
        // 0 — без отскока; 1 — без потери скорости при ударе.
        float restitution = 0.45f;

        // Ниже этой скорости удар считается остановкой.
        float restSpeed = 0.20f;
    };

    // Упрощённая физика отдельного тела над неподвижной платформой.
    //
    // Ограничения:
    // - движение только по Y;
    // - тело всегда перекрывает платформу по X/Z;
    // - столкновения с другими телами не обрабатываются;
    // - платформа неподвижна;
    // - боковые стороны и вращение не моделируются.
    //
    // Вызывать строго один раз за физический шаг.
    inline bool stepBouncingBox(
        const Platform& platform,
        Box& box,
        const ContactMaterial& material,
        float dt
    )
    {
        constexpr float GRAVITY = -9.81f;
        constexpr float MAX_DT = 0.02f;
        constexpr float POSITION_EPSILON = 0.0001f;

        if (!std::isfinite(dt) || dt <= 0.0f || dt > MAX_DT ||
            !std::isfinite(material.restitution) ||
            material.restitution < 0.0f ||
            material.restitution > 1.0f ||
            !std::isfinite(material.restSpeed) ||
            material.restSpeed < 0.0f ||
            !std::isfinite(box.y) ||
            !std::isfinite(box.velocityY) ||
            !std::isfinite(box.halfY) ||
            box.halfY <= 0.0f ||
            !std::isfinite(platform.y) ||
            !std::isfinite(platform.halfY) ||
            platform.halfY <= 0.0f)
        {
            return false;
        }

        const float platformTop =
            platform.y + platform.halfY;

        const float minimumY =
            platformTop + box.halfY;

        if (!std::isfinite(platformTop) ||
            !std::isfinite(minimumY) ||
            box.y < minimumY - POSITION_EPSILON)
        {
            return false;
        }

        // Убираем возможную очень небольшую ошибку округления.
        const float startY = std::max(box.y, minimumY);

        // Полунеявный Эйлер, как в текущем прототипе.
        const float nextVelocity = box.velocityY + GRAVITY * dt;
        const float nextY = startY + nextVelocity * dt;

        if (!std::isfinite(nextVelocity) ||
            !std::isfinite(nextY))
        {
            return false;
        }

        if (nextY <= minimumY && nextVelocity < 0.0f)
        {
            const float impactSpeed = -nextVelocity;

            box.y = minimumY;

            if (impactSpeed <= material.restSpeed)
            {
                box.velocityY = 0.0f;
            }
            else
            {
                box.velocityY =
                    impactSpeed * material.restitution;
            }
        }
        else
        {
            box.y = nextY;
            box.velocityY = nextVelocity;
        }

        return true;
    }
}
