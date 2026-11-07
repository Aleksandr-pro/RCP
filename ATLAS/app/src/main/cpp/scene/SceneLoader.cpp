#include "SceneLoader.h"

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
    bool loadScene(
        AAssetManager* assetManager,
        const char* path,
        std::vector<SceneObject>& objects
    )
    {
        constexpr off_t MAX_FILE_SIZE = 1024 * 1024;
        constexpr size_t MAX_OBJECTS = 10000;

        objects.clear();

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
                "Сцена не найдена: %s",
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
                "Некорректный размер сцены: %s",
                path
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
                    "Не удалось прочитать сцену: %s",
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

            std::string meshName;
            SceneObject object{};

            if (!(input
                >> meshName
                >> object.x
                >> object.y
                >> object.z
                >> object.scale
                >> object.rotationOffset))
            {
                __android_log_print(
                    ANDROID_LOG_ERROR,
                    LOG_TAG,
                    "Ошибка данных сцены: %s",
                    path
                );

                objects.clear();
                return false;
            }

            if (meshName == "pyramid")
            {
                object.mesh = MeshType::Pyramid;
            }
            else if (meshName == "cube")
            {
                object.mesh = MeshType::Cube;
            }
            else
            {
                __android_log_print(
                    ANDROID_LOG_ERROR,
                    LOG_TAG,
                    "Неизвестная модель в сцене %s: %s",
                    path,
                    meshName.c_str()
                );

                objects.clear();
                return false;
            }

            if (!std::isfinite(object.x) ||
                !std::isfinite(object.y) ||
                !std::isfinite(object.z) ||
                !std::isfinite(object.scale) ||
                !std::isfinite(object.rotationOffset) ||
                object.scale <= 0.0f)
            {
                __android_log_print(
                    ANDROID_LOG_ERROR,
                    LOG_TAG,
                    "Недопустимые параметры объекта в сцене: %s",
                    path
                );

                objects.clear();
                return false;
            }

            if (objects.size() >= MAX_OBJECTS)
            {
                __android_log_print(
                    ANDROID_LOG_ERROR,
                    LOG_TAG,
                    "Слишком много объектов в сцене: %s",
                    path
                );

                objects.clear();
                return false;
            }

            objects.push_back(object);
        }

        if (objects.empty())
        {
            __android_log_print(
                ANDROID_LOG_ERROR,
                LOG_TAG,
                "Сцена пуста: %s",
                path
            );

            return false;
        }

        __android_log_print(
            ANDROID_LOG_INFO,
            LOG_TAG,
            "Сцена загружена: %s, объектов: %u",
            path,
            static_cast<unsigned>(objects.size())
        );

        return true;
    }
}
