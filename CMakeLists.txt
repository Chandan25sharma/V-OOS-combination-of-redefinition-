cmake_minimum_required(VERSION 3.16)
project(VOS VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

# ─── Options ──────────────────────────────────────────────────
option(VOS_BUILD_TESTS "Build unit tests" ON)
option(VOS_BUILD_DESKTOP "Build desktop shell (SDL2 + ImGui)" ON)

# ─── Platform Detection ──────────────────────────────────────
if(WIN32)
    add_definitions(-DVOS_PLATFORM_WIN32)
    set(VOS_PLATFORM "win32")
elseif(ANDROID)
    add_definitions(-DVOS_PLATFORM_ANDROID)
    set(VOS_PLATFORM "android")
else()
    add_definitions(-DVOS_PLATFORM_LINUX)
    set(VOS_PLATFORM "linux")
endif()

# ─── Third-party: SDL2 ───────────────────────────────────────
if(VOS_BUILD_DESKTOP)
    # Try system SDL2 first, fall back to fetching
    find_package(SDL2 QUIET)
    if(NOT SDL2_FOUND)
        include(FetchContent)
        FetchContent_Declare(
            SDL2
            GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
            GIT_TAG        release-2.30.10
            GIT_SHALLOW    TRUE
        )
        set(SDL_SHARED OFF CACHE BOOL "" FORCE)
        set(SDL_STATIC ON  CACHE BOOL "" FORCE)
        set(SDL_TEST   OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(SDL2)
        set(SDL2_LIBRARIES SDL2-static SDL2main)
        set(SDL2_INCLUDE_DIRS ${sdl2_SOURCE_DIR}/include)
    else()
        set(SDL2_LIBRARIES SDL2::SDL2 SDL2::SDL2main)
    endif()
endif()

# ─── Third-party: ImGui ───────────────────────────────────────
if(VOS_BUILD_DESKTOP)
    include(FetchContent)
    FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG        v1.91.4
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(imgui)
    
    # We'll use SDL2 + OpenGL3 backends
    set(IMGUI_SOURCES 
        ${imgui_SOURCE_DIR}/imgui.cpp 
        ${imgui_SOURCE_DIR}/imgui_draw.cpp 
        ${imgui_SOURCE_DIR}/imgui_tables.cpp 
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    )
endif()

# ─── Core Library ─────────────────────────────────────────────
file(GLOB_RECURSE CORE_SOURCES
    src/core/*.cpp
    src/core/*.c
)
file(GLOB_RECURSE PLATFORM_SOURCES
    src/platform/${VOS_PLATFORM}/*.cpp
    src/platform/${VOS_PLATFORM}/*.c
)

add_library(vos_core STATIC
    ${CORE_SOURCES}
    ${PLATFORM_SOURCES}
)
target_include_directories(vos_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
if(WIN32)
    target_link_libraries(vos_core PUBLIC ws2_32 iphlpapi)
endif()

# ─── Desktop App ──────────────────────────────────────────────
if(VOS_BUILD_DESKTOP)
    file(GLOB_RECURSE SHELL_SOURCES src/shell/*.cpp)
    file(GLOB_RECURSE APP_SOURCES   src/apps/*.cpp)

    add_executable(vos_desktop
        ${SHELL_SOURCES}
        ${APP_SOURCES}
        ${IMGUI_SOURCES}
    )
    target_include_directories(vos_desktop PRIVATE
        ${SDL2_INCLUDE_DIRS}
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
    )
    target_link_libraries(vos_desktop PRIVATE
        vos_core
        ${SDL2_LIBRARIES}
        opengl32
    )
    if(WIN32)
        target_link_libraries(vos_desktop PRIVATE ws2_32 iphlpapi)
    endif()
endif()

# ─── Tests ────────────────────────────────────────────────────
if(VOS_BUILD_TESTS)
    enable_testing()
    file(GLOB TEST_SOURCES tests/*.cpp)
    foreach(test_file ${TEST_SOURCES})
        get_filename_component(test_name ${test_file} NAME_WE)
        add_executable(${test_name} ${test_file})
        target_link_libraries(${test_name} PRIVATE vos_core)
        target_include_directories(${test_name} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/third_party
        )
        add_test(NAME ${test_name} COMMAND ${test_name})
    endforeach()
endif()
