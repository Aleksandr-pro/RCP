#pragma once

#include "TerrainCollision.h"
#include "../physics/ContactMaterials.h"

namespace atlas::world
{
    // Временные пороги для теста, не геологическая модель.
    inline bool sampleTerrainMaterial(
        double worldX,
        double worldZ,
        atlas::physics::SurfaceMaterial& result
    )
    {
        double height = 0.0;

        if (!sampleTerrainMeshHeight(worldX, worldZ, height))
        {
            return false;
        }

        if (height < -0.20)
        {
            result = atlas::physics::SurfaceMaterial::Sand;
        }
        else if (height < 0.35)
        {
            result = atlas::physics::SurfaceMaterial::Ground;
        }
        else
        {
            result = atlas::physics::SurfaceMaterial::Stone;
        }

        return true;
    }
}
