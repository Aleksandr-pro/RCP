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

        // Каждые три вершины образуют отдельный треугольник.
        // Для него вычисляем одну плоскую нормаль.
        for (size_t i = 0; i < vertices.size(); i += 3)
        {
            const Vertex& a = vertices[i];
            const Vertex& b = vertices[i + 1];
            const Vertex& c = vertices[i + 2];

            // Промежуточные вычисления в double уменьшают риск переполнения
            // при обработке значений float из файла.
            const double abX = static_cast<double>(b.position[0]) - a.position[0];
            const double abY = static_cast<double>(b.position[1]) - a.position[1];
            const double abZ = static_cast<double>(b.position[2]) - a.position[2];

            const double acX = static_cast<double>(c.position[0]) - a.position[0];
            const double acY = static_cast<double>(c.position[1]) - a.position[1];
            const double acZ = static_cast<double>(c.position[2]) - a.position[2];

            const double nx = abY * acZ - abZ * acY;
            const double ny = abZ * acX - abX * acZ;
            const double nz = abX * acY - abY * acX;

            const double length = std::sqrt(nx * nx + ny * ny + nz * nz);

            if (!std::isfinite(length) || length <= 1e-12)
            {
                __android_log_print(
                    ANDROID_LOG_ERROR,
                    LOG_TAG,
                    "Вырожденный или некорректный треугольник в модели %s: %u",
                    path,
                    static_cast<unsigned>(i / 3)
                );

                vertices.clear();
                return false;
            }

            const float normal[3] = {
                static_cast<float>(nx / length),
                static_cast<float>(ny / length),
                static_cast<float>(nz / length)
            };

            for (size_t j = 0; j < 3; ++j)
            {
                vertices[i + j].normal[0] = normal[0];
                vertices[i + j].normal[1] = normal[1];
                vertices[i + j].normal[2] = normal[2];
            }
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
