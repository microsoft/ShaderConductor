# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.9)

if(WIN32 AND (${CMAKE_GENERATOR} MATCHES "Visual Studio ([^9]|[9][0-9])"))
    set(SC_WITH_CSHARP ON)
else()
    set(SC_WITH_CSHARP OFF)
endif()

set(PROJECT_NAME ShaderConductor)
project(${PROJECT_NAME})
if(SC_WITH_CSHARP)
    project(${PROJECT_NAME} LANGUAGES CSharp)
    set(CMAKE_CSharp_FLAGS "/langversion:7")
endif()

set(SC_MAJOR_VERSION 0)
set(SC_MINOR_VERSION 2)
set(SC_PATCH_VERSION 0)
set(SC_VERSION ${SC_MAJOR_VERSION}.${SC_MINOR_VERSION}.${SC_PATCH_VERSION})

set(SC_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(SC_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR})
mark_as_advanced(SC_ROOT_DIR)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${SC_BUILD_DIR}/Bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${SC_BUILD_DIR}/Lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${SC_BUILD_DIR}/Lib)

if((CMAKE_C_COMPILER_ID MATCHES GNU) OR (CMAKE_C_COMPILER_ID MATCHES Clang))
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1z")
endif()

set(Python_ADDITIONAL_VERSIONS 3.5 3.6 3.7)

# DirectXShaderCompiler requires it
add_definitions(-D_ITERATOR_DEBUG_LEVEL=0)
# Handle a compiling issue of SPIR-V
if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")

    foreach(flagVar
        CMAKE_SHARED_LINKER_FLAGS_RELEASE CMAKE_SHARED_LINKER_FLAGS_MINSIZEREL
        CMAKE_MODULE_LINKER_FLAGS_RELEASE CMAKE_MODULE_LINKER_FLAGS_MINSIZEREL
        CMAKE_EXE_LINKER_FLAGS_RELEASE CMAKE_EXE_LINKER_FLAGS_MINSIZEREL)
        set(${flagVar} "${${flagVar}} -s")
    endforeach()
endif()

if(WIN32)
    if(MSVC AND (CMAKE_GENERATOR MATCHES "^Visual Studio"))
        if((CMAKE_GENERATOR_PLATFORM STREQUAL "x64") OR (CMAKE_GENERATOR MATCHES "Win64"))
            set(SC_ARCH_NAME "x64")
        elseif((CMAKE_GENERATOR_PLATFORM STREQUAL "ARM64") OR (CMAKE_GENERATOR MATCHES "ARM64"))
            set(SC_ARCH_NAME "arm64")
        elseif((CMAKE_GENERATOR_PLATFORM STREQUAL "ARM") OR (CMAKE_GENERATOR MATCHES "ARM"))
            set(SC_ARCH_NAME "arm")
        else()
            set(SC_ARCH_NAME "x86")
        endif()
    endif()
else()
    if(NOT SC_ARCH_NAME)
        if((CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64") OR (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64"))
            set(SC_ARCH_NAME "x64")
        else()
            set(SC_ARCH_NAME "x86")
        endif()
    endif()
endif()

add_subdirectory(Source)
add_subdirectory(External)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT "ShaderConductorCmd")
