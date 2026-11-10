#pragma once

#include "ChunkGrid.h"
#include "TerrainHeight.h"
#include <cmath>

namespace atlas::world
{
    // 0.27b uses the 10-to-01 diagonal: triangles 00,01,10 and 10,01,11.
    constexpr int TERRAIN_CELLS_PER_CHUNK = 16;

    inline bool sampleTerrainMeshHeight(double worldX, double worldZ, double& result)
    {
        constexpr double CELL_SIZE = static_cast<double>(CHUNK_SIZE) / TERRAIN_CELLS_PER_CHUNK;
        constexpr double MAX_COORD = 100000.0;
        if (!std::isfinite(worldX) || !std::isfinite(worldZ) ||
            std::abs(worldX) > MAX_COORD || std::abs(worldZ) > MAX_COORD) return false;
        const double cellX = std::floor(worldX / CELL_SIZE);
        const double cellZ = std::floor(worldZ / CELL_SIZE);
        const double x0 = cellX * CELL_SIZE, z0 = cellZ * CELL_SIZE;
        const double x1 = x0 + CELL_SIZE, z1 = z0 + CELL_SIZE;
        const double u = (worldX - x0) / CELL_SIZE, v = (worldZ - z0) / CELL_SIZE;
        const double h00 = terrainHeight(x0, z0), h10 = terrainHeight(x1, z0);
        const double h01 = terrainHeight(x0, z1), h11 = terrainHeight(x1, z1);
        const double height = u + v <= 1.0
            ? (1.0 - u - v) * h00 + u * h10 + v * h01
            : (1.0 - v) * h10 + (1.0 - u) * h01 + (u + v - 1.0) * h11;
        if (!std::isfinite(height)) return false;
        result = height;
        return true;
    }
}
