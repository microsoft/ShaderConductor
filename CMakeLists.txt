# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

cmake_minimum_required(VERSION 3.9)

set (PROJECT_NAME ShaderConductor)

project(${PROJECT_NAME})
project(${PROJECT_NAME} LANGUAGES CSharp)
set(CMAKE_CSharp_FLAGS "/langversion:7")

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

# DirectXShaderCompiler requires it
add_definitions(-D_ITERATOR_DEBUG_LEVEL=0)
# Handle a compiling issue of SPIR-V
if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
endif()

add_subdirectory(Source)
add_subdirectory(External)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT "ShaderConductorCmd")
