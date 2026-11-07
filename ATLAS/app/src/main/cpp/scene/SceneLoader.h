#pragma once

#include "TestScene.h"

#include <android/asset_manager.h>

#include <vector>

namespace atlas
{
    // При ошибке objects остаётся пустым.
    bool loadScene(
        AAssetManager* assetManager,
        const char* path,
        std::vector<SceneObject>& objects
    );
}
