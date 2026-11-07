#pragma once

#include <string>

namespace atlas
{
    struct SceneObject
    {
        float x;
        float y;
        float z;

        float scale; // X (or uniform scale in legacy scenes)
        float scaleY;
        float scaleZ;
        float rotationOffset;

        // Имя файла модели без пути и расширения.
        std::string meshName;
    };
}
