#include "VerticalPhysics.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numeric>
#include <vector>

namespace
{
    constexpr float GRAVITY = -9.81f;
    constexpr float EPSILON = 0.00001f;

    bool validValue(float value)
    {
        return std::isfinite(value);
    }

    bool validBox(const atlas::physics::Box& box)
    {
        return
            validValue(box.x) &&
            validValue(box.y) &&
            validValue(box.z) &&
            validValue(box.halfX) &&
            validValue(box.halfY) &&
            validValue(box.halfZ) &&
            validValue(box.velocityY) &&
            box.halfX > 0.0f &&
            box.halfY > 0.0f &&
            box.halfZ > 0.0f;
    }

    bool validPlatform(
        const atlas::physics::Platform& platform
    )
    {
        return
            validValue(platform.x) &&
            validValue(platform.y) &&
            validValue(platform.z) &&
            validValue(platform.halfX) &&
            validValue(platform.halfY) &&
            validValue(platform.halfZ) &&
            platform.halfX > 0.0f &&
            platform.halfY > 0.0f &&
            platform.halfZ > 0.0f;
    }

    bool overlapsXZ(
        float ax,
        float az,
        float ahx,
        float ahz,
        float bx,
        float bz,
        float bhx,
        float bhz
    )
    {
        return
            std::abs(ax - bx) < ahx + bhx &&
            std::abs(az - bz) < ahz + bhz;
    }
}

namespace atlas::physics
{
    bool validate(
        const Platform& platform,
        const std::vector<Box>& boxes
    )
    {
        if (!validPlatform(platform))
        {
            return false;
        }

        for (const Box& box : boxes)
        {
            if (!validBox(box))
            {
                return false;
            }

            // В тестовой сцене каждый куб должен находиться
            // над платформой и пересекаться с ней по X/Z.
            if (!overlapsXZ(
                    box.x, box.z, box.halfX, box.halfZ,
                    platform.x, platform.z,
                    platform.halfX, platform.halfZ
                ))
            {
                return false;
            }

            const float bottom = box.y - box.halfY;
            const float platformTop = platform.y + platform.halfY;

            if (bottom < platformTop - EPSILON)
            {
                return false;
            }
        }

        // Начальные пересечения кубов не поддерживаются.
        for (size_t i = 0; i < boxes.size(); ++i)
        {
            for (size_t j = i + 1; j < boxes.size(); ++j)
            {
                const Box& a = boxes[i];
                const Box& b = boxes[j];

                const bool overlapY =
                    std::abs(a.y - b.y) <
                    a.halfY + b.halfY - EPSILON;

                if (overlapY &&
                    overlapsXZ(
                        a.x, a.z, a.halfX, a.halfZ,
                        b.x, b.z, b.halfX, b.halfZ
                    ))
                {
                    return false;
                }
            }
        }

        return true;
    }

    bool step(
        const Platform& platform,
        std::vector<Box>& boxes,
        float dt
    )
    {
        // Этот прототип рассчитан на малый фиксированный шаг.
        if (!std::isfinite(dt) || dt <= 0.0f || dt > 0.02f)
        {
            return false;
        }

        if (!validate(platform, boxes))
        {
            return false;
        }

        const size_t count = boxes.size();

        std::vector<Box> previous = boxes;
        std::vector<size_t> order(count);
        std::iota(order.begin(), order.end(), 0);

        // Сначала обновляем нижние тела. Для тел, которые
        // перекрываются по X/Z, начальных пересечений нет,
        // поэтому их вертикальный порядок определён.
        std::sort(
            order.begin(),
            order.end(),
            [&](size_t a, size_t b)
            {
                const float bottomA =
                    previous[a].y - previous[a].halfY;

                const float bottomB =
                    previous[b].y - previous[b].halfY;

                if (bottomA != bottomB)
                {
                    return bottomA < bottomB;
                }

                return a < b;
            }
        );

        const float platformTop = platform.y + platform.halfY;

        for (size_t orderIndex = 0;
             orderIndex < count;
             ++orderIndex)
        {
            const size_t index = order[orderIndex];

            Box& box = boxes[index];
            const Box& oldBox = previous[index];

            const float oldBottom = oldBox.y - oldBox.halfY;

            // Полунеявный Эйлер.
            box.velocityY = oldBox.velocityY + GRAVITY * dt;
            box.y = oldBox.y + box.velocityY * dt;

            float newBottom = box.y - box.halfY;

            bool hasSupport = false;
            float supportTop = 0.0f;
            float supportVelocityY = 0.0f;

            // Столкновение с неподвижной платформой.
            if (oldBottom >= platformTop - EPSILON &&
                newBottom <= platformTop)
            {
                hasSupport = true;
                supportTop = platformTop;
                supportVelocityY = 0.0f;
            }

            // Проверяем уже обновлённые нижние кубы.
            for (size_t lowerOrder = 0;
                 lowerOrder < orderIndex;
                 ++lowerOrder)
            {
                const size_t lowerIndex = order[lowerOrder];

                const Box& oldLower = previous[lowerIndex];
                const Box& newLower = boxes[lowerIndex];

                if (!overlapsXZ(
                        box.x, box.z, box.halfX, box.halfZ,
                        oldLower.x, oldLower.z,
                        oldLower.halfX, oldLower.halfZ
                    ))
                {
                    continue;
                }

                const float oldLowerTop =
                    oldLower.y + oldLower.halfY;

                const float newLowerTop =
                    newLower.y + newLower.halfY;

                // Проверяем пересечение за шаг с учётом
                // движения обеих поверхностей.
                if (oldBottom >= oldLowerTop - EPSILON &&
                    newBottom <= newLowerTop)
                {
                    if (!hasSupport || newLowerTop > supportTop)
                    {
                        hasSupport = true;
                        supportTop = newLowerTop;
                        supportVelocityY = newLower.velocityY;
                    }
                }
            }

            if (hasSupport)
            {
                box.y = supportTop + box.halfY;

                // Если опора ещё падает, верхний куб продолжает
                // движение вместе с ней. На платформе скорость 0.
                box.velocityY = supportVelocityY;
            }
        }

        return true;
    }
}
