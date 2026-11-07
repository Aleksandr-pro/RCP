#pragma once

#include <android/asset_manager.h>

#include <vector>

namespace atlas
{
    struct Vertex
    {
        float position[3];
        float color[3];
        float normal[3];
        float uv[2];
    };

    // Загружает список треугольников из assets.
    // При ошибке vertices остаётся пустым.
    bool loadMesh(
        AAssetManager* assetManager,
        const char* path,
        std::vector<Vertex>& vertices
    );
}
