#pragma once

#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace atlas::world
{
    struct ChunkCoord
    {
        int32_t x = 0;
        int32_t z = 0;
    };

    inline bool operator==(ChunkCoord a, ChunkCoord b)
    {
        return a.x == b.x && a.z == b.z;
    }

    // Размер тестового участка в единицах мира.
    constexpr float CHUNK_SIZE = 8.0f;

    // Находит участок по координатам мира.
    // floor важен для отрицательных координат:
    // -0.1 находится в участке -1, а не 0.
    inline bool worldToChunk(
        float worldX,
        float worldZ,
        ChunkCoord& result
    )
    {
        if (!std::isfinite(worldX) || !std::isfinite(worldZ))
        {
            return false;
        }

        const double x = std::floor(
            static_cast<double>(worldX) / CHUNK_SIZE
        );

        const double z = std::floor(
            static_cast<double>(worldZ) / CHUNK_SIZE
        );

        // Оставляем запас для добавления радиуса вокруг участка.
        constexpr int32_t LIMIT = 1000000;

        if (x < -LIMIT || x > LIMIT ||
            z < -LIMIT || z > LIMIT)
        {
            return false;
        }

        result = {
            static_cast<int32_t>(x),
            static_cast<int32_t>(z)
        };

        return true;
    }

    // Возвращает координаты участков квадратом вокруг центра.
    // Для radius = 1 это 3×3 = 9 участков.
    inline bool collectActiveChunks(
        ChunkCoord center,
        int32_t radius,
        std::vector<ChunkCoord>& result
    )
    {
        constexpr int32_t MAX_RADIUS = 4;
        constexpr int32_t LIMIT = 1000000;

        if (radius < 0 || radius > MAX_RADIUS ||
            center.x < -LIMIT || center.x > LIMIT ||
            center.z < -LIMIT || center.z > LIMIT)
        {
            return false;
        }

        std::vector<ChunkCoord> next;
        const int32_t width = radius * 2 + 1;
        next.reserve(static_cast<std::size_t>(width * width));

        for (int32_t dz = -radius; dz <= radius; ++dz)
        {
            for (int32_t dx = -radius; dx <= radius; ++dx)
            {
                next.push_back({
                    center.x + dx,
                    center.z + dz
                });
            }
        }

        result.swap(next);
        return true;
    }
}
