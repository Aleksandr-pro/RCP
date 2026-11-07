#pragma once

#include <android/asset_manager.h>

#include <cstdint>
#include <vector>

namespace atlas
{
    // Тестовый формат: ровно 8×8 пикселей RGBA8.
    // При ошибке pixels остаётся пустым.
    bool loadTestTexture(
        AAssetManager* assetManager,
        const char* path,
        std::vector<uint8_t>& pixels
    );
}
