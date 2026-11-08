#pragma once

#include "../physics/ContactMaterials.h"
#include <android/asset_manager.h>

namespace atlas
{
    // On failure, contact remains unchanged.
    bool loadMaterialConfig(AAssetManager* manager, const char* path,
                            physics::ContactMaterial& contact, float& friction);
}
