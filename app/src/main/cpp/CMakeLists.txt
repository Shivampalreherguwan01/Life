cmake_minimum_required(VERSION 3.22.1)
project(LIFE)

add_library(life SHARED
    main.cpp
    gltf_model.cpp
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
)

target_include_directories(life PRIVATE
    ${ANDROID_NDK}/sources/android/native_app_glue
)

target_link_libraries(life
    android
    log
    EGL
    GLESv3
)
