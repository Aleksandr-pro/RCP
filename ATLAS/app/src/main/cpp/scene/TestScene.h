#pragma once

#include <string>

namespace atlas
{
    struct SceneObject
    {
        float x;
        float y;
        float z;

        float scale;
        float rotationOffset;

        // Имя файла модели без пути и расширения.
        std::string meshName;
    };
}
