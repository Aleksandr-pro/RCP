#include "TextureLoader.h"

#include <android/log.h>

#include <cstddef>

namespace
{
    constexpr const char* LOG_TAG = "ATLAS";
    constexpr size_t TEXTURE_SIZE = 8 * 8 * 4;
}

namespace atlas
{
    bool loadTestTexture(
        AAssetManager* assetManager,
        const char* path,
        std::vector<uint8_t>& pixels
    )
    {
        pixels.clear();

        if (assetManager == nullptr || path == nullptr)
        {
            return false;
        }

        AAsset* asset = AAssetManager_open(
            assetManager,
            path,
            AASSET_MODE_STREAMING
        );

        if (asset == nullptr)
        {
            __android_log_print(
                ANDROID_LOG_ERROR,
                LOG_TAG,
                "Текстура не найдена: %s",
                path
            );
            return false;
        }

        const off_t length = AAsset_getLength(asset);

        if (length != static_cast<off_t>(TEXTURE_SIZE))
        {
            __android_log_print(
                ANDROID_LOG_ERROR,
                LOG_TAG,
                "Неверный размер текстуры %s: %lld, ожидалось %u",
                path,
                static_cast<long long>(length),
                static_cast<unsigned>(TEXTURE_SIZE)
            );

            AAsset_close(asset);
            return false;
        }

        pixels.resize(TEXTURE_SIZE);
        size_t totalRead = 0;

        while (totalRead < pixels.size())
        {
            const int bytesRead = AAsset_read(
                asset,
                pixels.data() + totalRead,
                pixels.size() - totalRead
            );

            if (bytesRead <= 0)
            {
                __android_log_print(
                    ANDROID_LOG_ERROR,
                    LOG_TAG,
                    "Не удалось прочитать текстуру: %s",
                    path
                );

                AAsset_close(asset);
                pixels.clear();
                return false;
            }

            totalRead += static_cast<size_t>(bytesRead);
        }

        AAsset_close(asset);

        uint32_t checksum = 0;

        for (uint8_t value : pixels)
        {
            checksum += value;
        }

        __android_log_print(
            ANDROID_LOG_INFO,
            LOG_TAG,
            "Текстура загружена: %s, 8x8 RGBA, байт: %u, сумма: %u",
            path,
            static_cast<unsigned>(pixels.size()),
            static_cast<unsigned>(checksum)
        );

        return true;
    }
}

namespace atlas
{
    bool loadOrnamentTexture(AAssetManager* manager, const char* path, std::vector<uint8_t>& pixels)
    {
        pixels.clear();
        constexpr size_t maxDimension = 4096;
        constexpr size_t maxBytes = 64 * 1024 * 1024;
        constexpr size_t width = ORNAMENT_WIDTH, height = ORNAMENT_HEIGHT;
        static_assert(width > 0 && height > 0 && width <= maxDimension && height <= maxDimension,
                      "invalid texture dimensions");
        static_assert(width <= maxBytes / 4 / height, "texture is too large");
        constexpr size_t size = width * height * 4;
        if (!manager || !path) return false;
        AAsset* asset = AAssetManager_open(manager, path, AASSET_MODE_STREAMING);
        if (!asset) return false;
        if (AAsset_getLength(asset) != static_cast<off_t>(size))
        {
            __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "Неверный размер текстуры: %s", path);
            AAsset_close(asset);
            return false;
        }
        pixels.resize(size);
        size_t offset = 0;
        while (offset < size)
        {
            int n = AAsset_read(asset, pixels.data() + offset, size - offset);
            if (n <= 0 || static_cast<size_t>(n) > size - offset)
            {
                pixels.clear();
                AAsset_close(asset);
                return false;
            }
            offset += static_cast<size_t>(n);
        }
        AAsset_close(asset);
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Текстура загружена: %s, %ux%u RGBA, байт: %u", path, ORNAMENT_WIDTH, ORNAMENT_HEIGHT, static_cast<unsigned>(size));
        return true;
    }
}

namespace atlas
{
    bool loadTerrainTexture(AAssetManager* manager, const char* path, std::vector<uint8_t>& pixels)
    {
        pixels.clear();
        constexpr size_t size = size_t(TERRAIN_TEXTURE_SIZE) * TERRAIN_TEXTURE_SIZE * 4;
        if (!manager || !path) return false;
        AAsset* asset = AAssetManager_open(manager, path, AASSET_MODE_STREAMING);
        if (!asset) { __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "Текстура не найдена: %s", path); return false; }
        if (AAsset_getLength(asset) != static_cast<off_t>(size))
        {
            __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "Неверный размер текстуры: %s", path);
            AAsset_close(asset);
            return false;
        }
        pixels.resize(size);
        size_t offset = 0;
        while (offset < size)
        {
            int n = AAsset_read(asset, pixels.data() + offset, size - offset);
            if (n <= 0 || static_cast<size_t>(n) > size - offset)
            {
                pixels.clear();
                AAsset_close(asset);
                return false;
            }
            offset += static_cast<size_t>(n);
        }
        AAsset_close(asset);
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Текстура загружена: %s, 512x512 RGBA, байт: %u", path, static_cast<unsigned>(size));
        return true;
    }
}
