cmake_minimum_required(VERSION 3.22.1)

project(ATLAS LANGUAGES C CXX)

set(NATIVE_APP_GLUE_DIR
    "${CMAKE_ANDROID_NDK}/sources/android/native_app_glue"
)

add_library(
    android_native_app_glue
    STATIC
    "${NATIVE_APP_GLUE_DIR}/android_native_app_glue.c"
)

target_include_directories(
    android_native_app_glue
    PUBLIC
    "${NATIVE_APP_GLUE_DIR}"
)

add_library(
    atlas
    SHARED
    main.cpp
    renderer/VulkanRenderer.cpp
    physics/FallingCube.cpp
    scene/MeshLoader.cpp
    scene/SceneLoader.cpp
    scene/TextureLoader.cpp
)

target_compile_features(
    atlas
    PRIVATE
    cxx_std_17
)

target_compile_options(
    atlas
    PRIVATE
    -Wall
    -Wextra
)

target_link_libraries(
    atlas
    PRIVATE
    android_native_app_glue
    android
    log
    vulkan
)
