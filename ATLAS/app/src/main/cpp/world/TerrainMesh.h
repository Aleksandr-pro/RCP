#pragma once

#include "ChunkGrid.h"
#include "TerrainHeight.h"
#include "TerrainCollision.h"
#include "../scene/Vertex.h"
#include <vector>

namespace atlas::world
{
    constexpr int TERRAIN_CELLS = TERRAIN_CELLS_PER_CHUNK;
    constexpr size_t TERRAIN_VERTICES = 9 * TERRAIN_CELLS * TERRAIN_CELLS * 6;

    // Coordinates are relative to the centre chunk; height, normals and UV use world coordinates.
    inline Vertex terrainVertex(ChunkCoord chunk, ChunkCoord center, int ix, int iz)
    {
        constexpr double step = static_cast<double>(CHUNK_SIZE) / TERRAIN_CELLS;
        const double x = static_cast<double>(chunk.x) * CHUNK_SIZE + ix * step;
        const double z = static_cast<double>(chunk.z) * CHUNK_SIZE + iz * step;
        const auto n = terrainNormal(x, z);
        return {{static_cast<float>(x - static_cast<double>(center.x) * CHUNK_SIZE),
                 static_cast<float>(terrainHeight(x, z)),
                 static_cast<float>(z - static_cast<double>(center.z) * CHUNK_SIZE)},
                {1.0f, 1.0f, 1.0f}, {n.x, n.y, n.z},
                {static_cast<float>(x / CHUNK_SIZE), static_cast<float>(z / CHUNK_SIZE)}};
    }

    inline bool buildTerrain(ChunkCoord center, std::vector<Vertex>& result)
    {
        std::vector<ChunkCoord> chunks;
        if (!collectActiveChunks(center, 1, chunks)) return false;
        std::vector<Vertex> next;
        next.reserve(TERRAIN_VERTICES);
        for (const auto chunk : chunks)
            for (int iz = 0; iz < TERRAIN_CELLS; ++iz)
                for (int ix = 0; ix < TERRAIN_CELLS; ++ix)
                {
                    const auto a = terrainVertex(chunk, center, ix, iz);
                    const auto b = terrainVertex(chunk, center, ix + 1, iz);
                    const auto c = terrainVertex(chunk, center, ix, iz + 1);
                    const auto d = terrainVertex(chunk, center, ix + 1, iz + 1);
                    // Cross product of each triangle's edges points up.
                    next.insert(next.end(), {a, c, b, b, c, d});
                }
        result.swap(next);
        return true;
    }
}
