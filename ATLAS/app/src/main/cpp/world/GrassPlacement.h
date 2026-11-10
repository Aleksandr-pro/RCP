#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace atlas::world {

struct GrassPlacement {
    double x;
    double y;
    double z;

    float height;
    float width;
    float rotationY;

    // Используется для стабильного прореживания.
    std::uint32_t id;
};

// Целочисленный хеш: одинаковый результат при повторной
// генерации, в том числе для отрицательных координат чанка.
inline std::uint32_t grassHash(
    std::int32_t chunkX,
    std::int32_t chunkZ,
    std::uint32_t cell,
    std::uint32_t salt
) {
    std::uint32_t h = 0x811C9DC5u;

    h = (h ^ static_cast<std::uint32_t>(chunkX)) * 0x01000193u;
    h = (h ^ static_cast<std::uint32_t>(chunkZ)) * 0x01000193u;
    h = (h ^ cell) * 0x01000193u;
    h = (h ^ salt) * 0x01000193u;

    h ^= h >> 16;
    h *= 0x7FEB352Du;
    h ^= h >> 15;
    h *= 0x846CA68Bu;
    h ^= h >> 16;

    return h;
}

inline float grassUnit(std::uint32_t value) {
    // Значение в диапазоне [0, 1).
    return static_cast<float>(value >> 8) * (1.0f / 16777216.0f);
}

// HeightSampler вызывается как sampleHeight(double x, double z).
// Он должен возвращать высоту фактической треугольной сетки,
// а не отдельно вычисленную высоту аналитической функции.
//
// out должен вмещать capacity элементов.
// Функция возвращает число записанных элементов.
template <typename HeightSampler>
std::size_t generateGrassForChunk(
    std::int32_t chunkX,
    std::int32_t chunkZ,
    double chunkSize,
    HeightSampler sampleHeight,
    GrassPlacement* out,
    std::size_t capacity
) {
    if (out == nullptr || capacity == 0 || chunkSize <= 0.0) {
        return 0;
    }

    constexpr std::uint32_t cellsPerAxis = 6;
    constexpr double twoPi = 6.28318530717958647692;

    const double cellSize = chunkSize / cellsPerAxis;
    const double originX = static_cast<double>(chunkX) * chunkSize;
    const double originZ = static_cast<double>(chunkZ) * chunkSize;

    std::size_t count = 0;

    for (std::uint32_t cz = 0; cz < cellsPerAxis; ++cz) {
        for (std::uint32_t cx = 0; cx < cellsPerAxis; ++cx) {
            const std::uint32_t cell = cz * cellsPerAxis + cx;

            // Пропускаем около 40% ячеек.
            const std::uint32_t presence =
                grassHash(chunkX, chunkZ, cell, 0);

            if (grassUnit(presence) < 0.40f) {
                continue;
            }

            if (count == capacity) {
                return count;
            }

            // Пучок находится внутри своей ячейки с отступом
            // от её границ, в том числе на границах чанков.
            const float offsetX =
                0.15f + 0.70f * grassUnit(
                    grassHash(chunkX, chunkZ, cell, 1)
                );

            const float offsetZ =
                0.15f + 0.70f * grassUnit(
                    grassHash(chunkX, chunkZ, cell, 2)
                );

            GrassPlacement p{};

            p.x = originX + (static_cast<double>(cx) + offsetX) * cellSize;
            p.z = originZ + (static_cast<double>(cz) + offsetZ) * cellSize;

            // Небольшое заглубление основания уменьшает щели
            // на склонах. Это не полноценное выравнивание по нормали.
            p.y = static_cast<double>(sampleHeight(p.x, p.z)) - 0.025;

            p.height = 0.18f + 0.17f * grassUnit(
                grassHash(chunkX, chunkZ, cell, 3)
            );

            p.width = 0.10f + 0.08f * grassUnit(
                grassHash(chunkX, chunkZ, cell, 4)
            );

            p.rotationY = static_cast<float>(
                twoPi * grassUnit(grassHash(chunkX, chunkZ, cell, 5))
            );

            p.id = grassHash(chunkX, chunkZ, cell, 6);
            out[count++] = p;
        }
    }

    return count;
}

// Простое дистанционное прореживание.
// Расстояние измеряется по горизонтали до камеры.
inline bool isGrassVisible(
    const GrassPlacement& grass,
    double cameraX,
    double cameraZ
) {
    const double dx = grass.x - cameraX;
    const double dz = grass.z - cameraZ;
    const double distanceSquared = dx * dx + dz * dz;

    constexpr double nearDistance = 10.0;
    constexpr double farDistance = 18.0;

    if (distanceSquared <= nearDistance * nearDistance) {
        return true;
    }

    if (distanceSquared <= farDistance * farDistance) {
        // На средней дистанции оставляем примерно половину.
        return (grass.id & 1u) == 0u;
    }

    return false;
}

} // namespace atlas::world
