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
