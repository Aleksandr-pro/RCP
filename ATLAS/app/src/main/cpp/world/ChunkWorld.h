#pragma once
#include "ChunkGrid.h"

namespace atlas::world
{
    class ChunkWorld
    {
    public:
        bool moveTo(float x, float z, bool& changed)
        {
            ChunkCoord next{};
            if (!worldToChunk(x, z, next)) return false;
            const bool transition = !initialized_ || !(next == center_);
            if (transition)
            {
                std::vector<ChunkCoord> chunks;
                if (!collectActiveChunks(next, 1, chunks)) return false;
                active_.swap(chunks);
                center_ = next;
                initialized_ = true;
            }
            x_ = x;
            z_ = z;
            changed = transition;
            return true;
        }
        float x() const { return x_; }
        float z() const { return z_; }
        ChunkCoord center() const { return center_; }
        const std::vector<ChunkCoord>& active() const { return active_; }
    private:
        float x_ = 0, z_ = 0;
        bool initialized_ = false;
        ChunkCoord center_{};
        std::vector<ChunkCoord> active_;
    };
}
