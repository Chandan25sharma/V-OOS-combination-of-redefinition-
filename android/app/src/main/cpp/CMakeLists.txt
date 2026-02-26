cmake_minimum_required(VERSION 3.22)
project(vos_android LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

add_definitions(-DVOS_PLATFORM_ANDROID)

# ─── VOS Core Sources ─────────────────────────────────────────
set(VOS_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../../../..)

file(GLOB_RECURSE CORE_SOURCES
    ${VOS_ROOT}/src/core/*.cpp
    ${VOS_ROOT}/src/core/*.c
)

file(GLOB_RECURSE PLATFORM_SOURCES
    ${VOS_ROOT}/src/platform/android/*.cpp
)

file(GLOB_RECURSE APP_SOURCES
    ${VOS_ROOT}/src/apps/*.cpp
)

# ─── JNI Bridge ───────────────────────────────────────────────
set(JNI_BRIDGE_SOURCES
    vos_jni_bridge.cpp
)

# ─── Shared Library (loaded by Android) ───────────────────────
add_library(vos_native SHARED
    ${CORE_SOURCES}
    ${PLATFORM_SOURCES}
    ${APP_SOURCES}
    ${JNI_BRIDGE_SOURCES}
)

target_include_directories(vos_native PRIVATE
    ${VOS_ROOT}/include
    ${VOS_ROOT}/src
)

# Link Android-specific libraries
find_library(log-lib log)
find_library(android-lib android)

target_link_libraries(vos_native
    ${log-lib}
    ${android-lib}
)
