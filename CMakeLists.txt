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

# ---------------------------------------------------------------------------
# Optional OptiX GPU backend. Off by default so the CPU build needs nothing but
# a C++ compiler. The device programs are compiled straight to PTX with nvcc,
# which keeps the C++ build on the regular compiler.
# ---------------------------------------------------------------------------
option(TDSG_OPTIX "Build the OptiX GPU backend" OFF)
set(TDSG_OPTIX_SDK_DIR "" CACHE PATH "OptiX SDK root (the folder that contains include/optix.h)")
set(TDSG_OPTIX_SM "89" CACHE STRING "CUDA SM architecture used to compile the OptiX programs")
set(TDSG_OPTIX_HOST_COMPILER "" CACHE PATH "Folder that contains cl.exe, used by nvcc as host compiler")

if(TDSG_OPTIX)
    find_package(CUDAToolkit REQUIRED)

    if(NOT TDSG_OPTIX_SDK_DIR)
        if(DEFINED ENV{OPTIX_SDK_DIR})
            set(TDSG_OPTIX_SDK_DIR "$ENV{OPTIX_SDK_DIR}")
        else()
            message(FATAL_ERROR "TDSG_OPTIX requires TDSG_OPTIX_SDK_DIR (or the OPTIX_SDK_DIR environment variable)")
        endif()
    endif()

    # nvcc needs cl.exe on Windows; it is usually not on PATH.
    if(WIN32 AND NOT TDSG_OPTIX_HOST_COMPILER)
        find_program(TDSG_CL_EXE NAMES cl)
        if(TDSG_CL_EXE)
            get_filename_component(TDSG_OPTIX_HOST_COMPILER "${TDSG_CL_EXE}" DIRECTORY)
        else()
            file(GLOB _tdsg_cl_candidates
                "C:/Program Files/Microsoft Visual Studio/*/*/VC/Tools/MSVC/*/bin/Hostx64/x64/cl.exe"
                "C:/Program Files (x86)/Microsoft Visual Studio/*/*/VC/Tools/MSVC/*/bin/Hostx64/x64/cl.exe")
            if(_tdsg_cl_candidates)
                list(GET _tdsg_cl_candidates 0 _tdsg_cl)
                get_filename_component(TDSG_OPTIX_HOST_COMPILER "${_tdsg_cl}" DIRECTORY)
            else()
                message(FATAL_ERROR "TDSG_OPTIX: set TDSG_OPTIX_HOST_COMPILER to the folder containing cl.exe")
            endif()
        endif()
        message(STATUS "TDSG_OPTIX: nvcc host compiler ${TDSG_OPTIX_HOST_COMPILER}")
    endif()

    if(TDSG_SPECTRAL)
        set(TDSG_SPECTRAL_DEFINE 1)
    else()
        set(TDSG_SPECTRAL_DEFINE 0)
    endif()

    set(TDSG_OPTIX_PTX "${CMAKE_CURRENT_BINARY_DIR}/tdsg_optix.ptx")
    file(TO_CMAKE_PATH "${TDSG_OPTIX_PTX}" TDSG_OPTIX_PTX_FWD)
    file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/src" TDSG_OPTIX_SRC_FWD)
    file(TO_CMAKE_PATH "${TDSG_OPTIX_SDK_DIR}/include" TDSG_OPTIX_SDK_INCLUDE_FWD)

    set(TDSG_NVCC_FLAGS -ptx -std=c++17 "-arch=sm_${TDSG_OPTIX_SM}" --expt-relaxed-constexpr
        "-DTDSG_SPECTRAL=${TDSG_SPECTRAL_DEFINE}")
    if(WIN32)
        list(APPEND TDSG_NVCC_FLAGS "-ccbin" "${TDSG_OPTIX_HOST_COMPILER}")
    endif()

    add_custom_command(
        OUTPUT "${TDSG_OPTIX_PTX}"
        COMMAND "${CUDAToolkit_NVCC_EXECUTABLE}" ${TDSG_NVCC_FLAGS}
                "-I${TDSG_OPTIX_SRC_FWD}"
                "-I${TDSG_OPTIX_SDK_INCLUDE_FWD}"
                "${TDSG_OPTIX_SRC_FWD}/optix/shaders.cu"
                -o "${TDSG_OPTIX_PTX_FWD}"
        DEPENDS src/optix/shaders.cu src/optix/device_scene.h src/optix/device_shading.cuh
        COMMENT "Compiling the TDSG OptiX programs to PTX"
        VERBATIM)
    add_custom_target(tdsg_optix_ptx DEPENDS "${TDSG_OPTIX_PTX}")

    add_library(tdsg_optix src/optix/optix_backend.cpp src/optix/scene_upload.cpp)
    add_dependencies(tdsg_optix tdsg_optix_ptx)
    target_include_directories(tdsg_optix SYSTEM PRIVATE "${TDSG_OPTIX_SDK_DIR}/include")
    target_include_directories(tdsg_optix PRIVATE ${CUDAToolkit_INCLUDE_DIRS})
    target_compile_definitions(tdsg_optix PRIVATE
        TDSG_HAS_OPTIX=1
        "TDSG_OPTIX_PTX=\"${TDSG_OPTIX_PTX_FWD}\"")
    target_link_libraries(tdsg_optix PUBLIC tdsg PRIVATE CUDA::cuda_driver)
    if(MSVC)
        target_compile_options(tdsg_optix PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(tdsg_optix PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    target_link_libraries(tdsg_render PRIVATE tdsg_optix)
    target_compile_definitions(tdsg_render PRIVATE TDSG_HAS_OPTIX=1)
endif()
