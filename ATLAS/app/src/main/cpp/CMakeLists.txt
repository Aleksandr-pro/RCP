cmake_minimum_required(VERSION 3.22.1)
project(ATLAS LANGUAGES C CXX)

add_library(atlas SHARED
    main.cpp
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
)
target_include_directories(atlas PRIVATE
    ${ANDROID_NDK}/sources/android/native_app_glue
)
target_link_libraries(atlas PRIVATE android log)
