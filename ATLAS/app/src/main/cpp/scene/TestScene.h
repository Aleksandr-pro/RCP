#pragma once

#include <cstddef>

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

    // Временная тестовая сцена.
    // Позже данные будут загружаться из файлов карты.
    inline constexpr SceneObject TEST_SCENE[] = {
        {-0.68f, -0.68f, 0.0f, 0.42f, -0.30f, MeshType::Pyramid},
        { 0.00f, -0.68f, 0.0f, 0.42f, -0.15f, MeshType::Cube},
        { 0.68f, -0.68f, 0.0f, 0.42f,  0.00f, MeshType::Pyramid},

        {-0.68f,  0.00f, 0.0f, 0.42f, -0.15f, MeshType::Cube},
        { 0.00f,  0.00f, 0.0f, 0.42f,  0.00f, MeshType::Pyramid},
        { 0.68f,  0.00f, 0.0f, 0.42f,  0.15f, MeshType::Cube},

        {-0.68f,  0.68f, 0.0f, 0.42f,  0.00f, MeshType::Pyramid},
        { 0.00f,  0.68f, 0.0f, 0.42f,  0.15f, MeshType::Cube},
        { 0.68f,  0.68f, 0.0f, 0.42f,  0.30f, MeshType::Pyramid}
    };

    inline constexpr std::size_t TEST_SCENE_OBJECT_COUNT =
        sizeof(TEST_SCENE) / sizeof(TEST_SCENE[0]);
}
