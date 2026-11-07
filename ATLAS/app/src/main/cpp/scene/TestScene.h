#pragma once

namespace atlas
{
    enum class MeshType
    {
        Pyramid,
        Cube
    };

    struct SceneObject
    {
        float x;
        float y;
        float z;

        float scale;
        float rotationOffset;

        MeshType mesh;
    };
}
