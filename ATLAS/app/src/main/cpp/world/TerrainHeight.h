#pragma once

#include <cmath>

namespace atlas::world
{
    // Тестовая высота рельефа в мировых координатах.
    // Одинаковые координаты всегда дают одинаковую высоту.
    inline double terrainHeight(double worldX, double worldZ)
    {
        return
            0.70 * std::sin(worldX * 0.10) *
                   std::cos(worldZ * 0.10)
          + 0.25 * std::sin(worldX * 0.23 + worldZ * 0.17);
    }
}

namespace atlas::world
{
    struct TerrainNormal
    {
        float x;
        float y;
        float z;
    };

    inline TerrainNormal terrainNormal(double worldX, double worldZ)
    {
        constexpr double E = 0.05;

        const double dx =
            (terrainHeight(worldX + E, worldZ) -
             terrainHeight(worldX - E, worldZ)) / (2.0 * E);

        const double dz =
            (terrainHeight(worldX, worldZ + E) -
             terrainHeight(worldX, worldZ - E)) / (2.0 * E);

        const double length = std::sqrt(dx * dx + 1.0 + dz * dz);

        return {
            static_cast<float>(-dx / length),
            static_cast<float>(1.0 / length),
            static_cast<float>(-dz / length)
        };
    }
}
