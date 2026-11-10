cmake_minimum_required(VERSION 3.16)
project(TDSG-Renderer LANGUAGES CXX)
find_package(Threads REQUIRED)
option(TDSG_SPECTRAL "spectral rendering!!!" OFF)

add_library(tdsg
    src/color/spectrum.cpp
    src/geometry/primitive.cpp
    src/geometry/bvh.cpp
    src/material/material.cpp
    src/scene/scene.cpp
    src/scene/make_scene.cpp
    src/scene/obj_loader.cpp
    src/integrator/integrator.cpp
    src/film/film.cpp
    src/render/render.cpp
    src/light/lights.cpp
    src/light/point_light.cpp
    src/light/area_light.cpp
    src/light/spot_light.cpp
)
target_compile_features(tdsg PUBLIC cxx_std_17)
target_include_directories(tdsg PUBLIC src)
target_include_directories(tdsg SYSTEM PRIVATE third_party)
target_compile_definitions(tdsg PRIVATE TDSG_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")
target_compile_definitions(tdsg PUBLIC TDSG_SPECTRAL=$<BOOL:${TDSG_SPECTRAL}>)
target_link_libraries(tdsg PUBLIC Threads::Threads)
if(MSVC)
    target_compile_options(tdsg PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(tdsg PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(tdsg_render src/main.cpp)
target_link_libraries(tdsg_render PRIVATE tdsg)
