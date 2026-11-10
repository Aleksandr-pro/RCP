#pragma once
#include "StonePlacement.h"

namespace atlas::world
{
    // Replace atomically: at most 9 chunks × 16 stones, never accumulated.
    inline bool buildActiveStones(ChunkCoord center, std::vector<StonePlacement>& result)
    {
        std::vector<ChunkCoord> chunks;
        if (!collectActiveChunks(center, 1, chunks)) return false;
        std::vector<StonePlacement> next;
        next.reserve(9 * 16);
        for (auto chunk : chunks)
        {
            std::vector<StonePlacement> stones;
            if (!generateStonePlacements(chunk, stones)) return false;
            next.insert(next.end(), stones.begin(), stones.end());
        }
        result.swap(next);
        return true;
    }
}
