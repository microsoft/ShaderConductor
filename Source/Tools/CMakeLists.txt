# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

set(EXE_NAME ShaderConductorCmd)

set(SOURCE_FILES
    ShaderConductorCmd.cpp
)

source_group("Source Files" FILES ${SOURCE_FILES})

add_executable(${EXE_NAME} ${SOURCE_FILES})

target_link_libraries(${EXE_NAME}
    PRIVATE
        ShaderConductor
        cxxopts
)

add_dependencies(${EXE_NAME} ShaderConductor)

set_target_properties(${EXE_NAME} PROPERTIES FOLDER "Tools")
