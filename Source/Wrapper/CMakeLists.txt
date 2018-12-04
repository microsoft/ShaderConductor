# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

set(DLL_NAME ShaderConductorWrapper)

set(SOURCE_FILES
    Native.cpp
    Native.h
)

source_group("Source Files" FILES ${SOURCE_FILES})

add_library(${DLL_NAME} SHARED ${SOURCE_FILES})

target_include_directories(${DLL_NAME} PRIVATE
    ${SC_ROOT_DIR}/Include
)

target_link_libraries(${DLL_NAME} ShaderConductor)

add_dependencies(${DLL_NAME} ShaderConductor)

set_target_properties(${DLL_NAME} PROPERTIES FOLDER "Wrapper")
