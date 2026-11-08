#pragma once

#include "ContactMaterials.h"

#include <string>

namespace atlas::physics
{
    inline bool parseBodyMaterial(
        const std::string& name,
        BodyMaterial& result
    )
    {
        if (name == "test_rubber")
        {
            result = BodyMaterial::TestRubber;
            return true;
        }

        if (name == "test_wood")
        {
            result = BodyMaterial::TestWood;
            return true;
        }

        return false;
    }

    inline bool parseSurfaceMaterial(
        const std::string& name,
        SurfaceMaterial& result
    )
    {
        if (name == "stone")
        {
            result = SurfaceMaterial::Stone;
            return true;
        }

        if (name == "ground")
        {
            result = SurfaceMaterial::Ground;
            return true;
        }

        if (name == "sand")
        {
            result = SurfaceMaterial::Sand;
            return true;
        }

        return false;
    }
}
