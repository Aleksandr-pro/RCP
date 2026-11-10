#pragma once

#include "ChunkGrid.h"
#include "TerrainCollision.h"

#include <cstdint>
#include <vector>

namespace atlas::world
{
    struct StonePlacement
    {
        double x;
        double y;
        double z;

        float scale;
        float rotationY;
    };

    // Детерминированное перемешивание целых чисел.
    inline uint32_t stoneHash(uint32_t value)
    {
        value ^= value >> 16;
        value *= 0x7feb352du;
        value ^= value >> 15;
        value *= 0x846ca68bu;
        value ^= value >> 16;
        return value;
    }

    // Число в диапазоне [0, 1).
    inline float stoneRandom01(uint32_t seed)
    {
        return static_cast<float>(
            stoneHash(seed) & 0x00ffffffu
        ) / 16777216.0f;
    }

    // Создаёт до 16 камешков для одного участка.
    // При ошибке result не изменяется.
    inline bool generateStonePlacements(
        ChunkCoord chunk,
        std::vector<StonePlacement>& result
    )
    {
        constexpr int GRID_SIZE = 4;
        constexpr float PI = 3.14159265358979323846f;

        // Соответствует ограничению sampleTerrainMeshHeight().
        // С запасом: вся площадь участка должна быть внутри
        // допустимого диапазона координат.
        constexpr int32_t MAX_CHUNK = 12000;

        if (chunk.x < -MAX_CHUNK || chunk.x > MAX_CHUNK ||
            chunk.z < -MAX_CHUNK || chunk.z > MAX_CHUNK)
        {
            return false;
        }

        std::vector<StonePlacement> next;
        next.reserve(GRID_SIZE * GRID_SIZE);

        const double originX =
            static_cast<double>(chunk.x) * CHUNK_SIZE;

        const double originZ =
            static_cast<double>(chunk.z) * CHUNK_SIZE;

        const double cellSize =
            static_cast<double>(CHUNK_SIZE) / GRID_SIZE;

        for (int cellZ = 0; cellZ < GRID_SIZE; ++cellZ)
        {
            for (int cellX = 0; cellX < GRID_SIZE; ++cellX)
            {
                // Преобразование знакового числа в uint32_t
                // определено C++ и одинаково для отрицательных координат.
                const uint32_t chunkSeed =
                    stoneHash(static_cast<uint32_t>(chunk.x)) ^
                    stoneHash(
                        static_cast<uint32_t>(chunk.z) ^ 0x9e3779b9u
                    );

                const uint32_t cellIndex =
                    static_cast<uint32_t>(cellZ * GRID_SIZE + cellX);

                const uint32_t seed =
                    stoneHash(chunkSeed ^ stoneHash(cellIndex));

                // Пропускаем часть ячеек для более свободного рисунка.
                if (stoneRandom01(seed ^ 0x12a3u) < 0.35f)
                {
                    continue;
                }

                // Небольшой отступ от границы ячейки.
                const double offsetX =
                    0.1 + 0.8 * stoneRandom01(seed ^ 0x4567u);

                const double offsetZ =
                    0.1 + 0.8 * stoneRandom01(seed ^ 0x89abu);

                const double x =
                    originX + (cellX + offsetX) * cellSize;

                const double z =
                    originZ + (cellZ + offsetZ) * cellSize;

                double groundY = 0.0;

                if (!sampleTerrainMeshHeight(x, z, groundY))
                {
                    return false;
                }

                const float scale =
                    0.10f + 0.14f * stoneRandom01(seed ^ 0xcdefu);

                const float rotation =
                    2.0f * PI * stoneRandom01(seed ^ 0x1357u);

                next.push_back({
                    x,
                    groundY,
                    z,
                    scale,
                    rotation
                });
            }
        }

        result.swap(next);
        return true;
    }
}
