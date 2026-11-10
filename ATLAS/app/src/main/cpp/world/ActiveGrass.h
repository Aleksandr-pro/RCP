#pragma once
#include "GrassPlacement.h"
#include "ChunkGrid.h"
#include "TerrainCollision.h"
#include <array>
#include <vector>

namespace atlas::world {
// Transactional replacement: only the current nine chunks are retained.
inline bool buildActiveGrass(ChunkCoord center, std::vector<GrassPlacement>& result) {
    std::vector<ChunkCoord> chunks;
    if (!collectActiveChunks(center, 1, chunks)) return false;
    std::vector<GrassPlacement> next;
    next.reserve(9 * 36);
    for (auto chunk : chunks) {
        // Same supported range as the existing stones and mesh-height sampler.
        if (chunk.x < -12000 || chunk.x > 12000 || chunk.z < -12000 || chunk.z > 12000) return false;
        bool valid = true;
        auto sample = [&valid](double x, double z) {
            double y = 0.0;
            if (!sampleTerrainMeshHeight(x, z, y)) valid = false;
            return y;
        };
        std::array<GrassPlacement, 36> grass{};
        const size_t count = generateGrassForChunk(chunk.x, chunk.z, CHUNK_SIZE, sample, grass.data(), grass.size());
        if (!valid) return false;
        next.insert(next.end(), grass.begin(), grass.begin() + count);
    }
    result.swap(next);
    return true;
}

// Inverse of cameraView(distance) * rotationX(tilt) * rotationY(yaw).
// Observer coordinates and camera distance use different scene scales.
inline void worldCameraXZ(double observerX, double observerZ, float yaw, float tilt,
                          float distance, float worldScale, double& x, double& z) {
    const double horizontal = double(distance) * std::cos(double(tilt)) / worldScale;
    x = observerX + std::sin(double(yaw)) * horizontal;
    z = observerZ - std::cos(double(yaw)) * horizontal;
}
}
