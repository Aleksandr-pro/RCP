#pragma once

#include "../world/TerrainCollision.h"

#include <cmath>

namespace atlas::physics
{
    struct TerrainBody
    {
        double x = 0.0;
        double y = 0.0;
        double z = 0.0;

        double halfHeight = 0.5;
        double velocityY = 0.0;
    };

    // Узкий тест: тело движется только вертикально.
    // Отскок рассчитывается относительно неподвижного рельефа.
    //
    // При ошибке body остаётся без изменений.
    inline bool stepTerrainBody(
        TerrainBody& body,
        double restitution,
        double restSpeed,
        double dt
    )
    {
        constexpr double GRAVITY = -9.81;
        constexpr double MAX_DT = 0.02;
        constexpr double POSITION_EPSILON = 0.0001;

        if (!std::isfinite(body.x) ||
            !std::isfinite(body.y) ||
            !std::isfinite(body.z) ||
            !std::isfinite(body.halfHeight) ||
            body.halfHeight <= 0.0 ||
            !std::isfinite(body.velocityY) ||
            !std::isfinite(restitution) ||
            restitution < 0.0 ||
            restitution > 1.0 ||
            !std::isfinite(restSpeed) ||
            restSpeed < 0.0 ||
            !std::isfinite(dt) ||
            dt <= 0.0 ||
            dt > MAX_DT)
        {
            return false;
        }

        double groundHeight = 0.0;

        if (!atlas::world::sampleTerrainMeshHeight(
                body.x,
                body.z,
                groundHeight
            ))
        {
            return false;
        }

        const double minimumY = groundHeight + body.halfHeight;

        if (!std::isfinite(minimumY) ||
            body.y < minimumY - POSITION_EPSILON)
        {
            return false;
        }

        TerrainBody next = body;

        // Корректируем только очень маленькое проникновение,
        // вызванное округлением.
        if (next.y < minimumY)
        {
            next.y = minimumY;
        }

        // Полунеявный Эйлер, как в существующем прототипе.
        next.velocityY += GRAVITY * dt;
        next.y += next.velocityY * dt;

        if (!std::isfinite(next.y) ||
            !std::isfinite(next.velocityY))
        {
            return false;
        }

        if (next.y <= minimumY && next.velocityY < 0.0)
        {
            const double impactSpeed = -next.velocityY;

            next.y = minimumY;
            next.velocityY =
                impactSpeed <= restSpeed
                    ? 0.0
                    : impactSpeed * restitution;
        }

        body = next;
        return true;
    }
}
