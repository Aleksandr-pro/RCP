#include "MeshLoader.h"

#include <android/log.h>

#include <cmath>
#include <locale>
#include <sstream>
#include <string>

namespace
{
    constexpr const char* LOG_TAG = "ATLAS";
}

namespace atlas
{
    bool loadMesh(
        AAssetManager* assetManager,
        const char* path,
        std::vector<Vertex>& vertices
    )
    {
        constexpr off_t MAX_FILE_SIZE = 1024 * 1024;
        constexpr size_t MAX_VERTICES = 30000;

        vertices.clear();

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
                "Модель не найдена: %s",
                path
            );
            return false;
        }

        const off_t length = AAsset_getLength(asset);

        if (length <= 0 || length > MAX_FILE_SIZE)
        {
            __android_log_print(
                ANDROID_LOG_ERROR,
                LOG_TAG,
                "Некорректный размер модели %s: %lld",
                path,
                static_cast<long long>(length)
            );

            AAsset_close(asset);
            return false;
        }

        std::string contents(static_cast<size_t>(length), '\0');
        size_t totalRead = 0;

        while (totalRead < contents.size())
        {
            const int bytesRead = AAsset_read(
                asset,
                &contents[totalRead],
                contents.size() - totalRead
            );

            if (bytesRead <= 0)
            {
                __android_log_print(
                    ANDROID_LOG_ERROR,
                    LOG_TAG,
                    "Не удалось полностью прочитать модель: %s",
                    path
                );

                AAsset_close(asset);
                return false;
            }

            totalRead += static_cast<size_t>(bytesRead);
        }

        AAsset_close(asset);

        std::istringstream input(contents);
        input.imbue(std::locale::classic());

        while (true)
        {
            input >> std::ws;

            if (input.eof())
            {
                break;
            }

            Vertex vertex{};

            if (!(input
                >> vertex.position[0]
                >> vertex.position[1]
                >> vertex.position[2]
                >> vertex.color[0]
                >> vertex.color[1]
                >> vertex.color[2]))
            {
                __android_log_print(
                    ANDROID_LOG_ERROR,
                    LOG_TAG,
                    "Ошибка данных модели: %s",
                    path
                );
                vertices.clear();

                return false;
            }

            for (int i = 0; i < 3; ++i)
            {
                if (!std::isfinite(vertex.position[i]) ||
                    !std::isfinite(vertex.color[i]) ||
                    vertex.color[i] < 0.0f ||
                    vertex.color[i] > 1.0f)
                {
                    __android_log_print(
                        ANDROID_LOG_ERROR,
                        LOG_TAG,
                        "Недопустимая вершина модели: %s",
                        path
                    );
                    vertices.clear();
                    return false;
                }
            }

            if (vertices.size() >= MAX_VERTICES)
            {
                __android_log_print(
                    ANDROID_LOG_ERROR,
                    LOG_TAG,
                    "Слишком много вершин в модели: %s",
                    path
                );
                vertices.clear();
                return false;
            }

            vertices.push_back(vertex);
        }

        if (vertices.empty() || vertices.size() % 3 != 0)
        {
            __android_log_print(
                ANDROID_LOG_ERROR,
                LOG_TAG,
                "Модель пуста или число вершин не кратно трём: %s",
                path
            );
            vertices.clear();
            return false;
        }

        __android_log_print(
            ANDROID_LOG_INFO,
            LOG_TAG,
            "Модель загружена: %s, вершин: %u",
            path,
            static_cast<unsigned>(vertices.size())
        );

        return true;
    }
}
