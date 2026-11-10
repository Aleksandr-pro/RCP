#pragma once

#include <android/asset_manager.h>
#include "Vertex.h"

#include <vector>

namespace atlas
{
    // Загружает список треугольников из assets.
    // При ошибке vertices остаётся пустым.
    bool loadMesh(
        AAssetManager* assetManager,
        const char* path,
        std::vector<Vertex>& vertices
    );
}
