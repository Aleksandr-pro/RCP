#pragma once

#include <android/asset_manager.h>

#include <cstdint>
#include <vector>

namespace atlas
{
    constexpr uint32_t ORNAMENT_WIDTH = 1024;
    constexpr uint32_t ORNAMENT_HEIGHT = 559;
    bool loadOrnamentTexture(AAssetManager* assetManager, const char* path, std::vector<uint8_t>& pixels);

    // Тестовый формат: ровно 8×8 пикселей RGBA8.
    // При ошибке pixels остаётся пустым.
    bool loadTestTexture(
        AAssetManager* assetManager,
        const char* path,
        std::vector<uint8_t>& pixels
    );
}
