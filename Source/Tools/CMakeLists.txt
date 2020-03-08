# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

set(EXE_NAME ShaderConductorCmd)

set(SOURCE_FILES
    ShaderConductorCmd.cpp
)

source_group("Source Files" FILES ${SOURCE_FILES})

add_executable(${EXE_NAME} ${SOURCE_FILES})

target_include_directories(${EXE_NAME}
    PRIVATE
        $<TARGET_PROPERTY:ShaderConductor,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:cxxopts,INTERFACE_INCLUDE_DIRECTORIES>
)
target_link_libraries(${EXE_NAME}
    PRIVATE
        ShaderConductor
)

add_dependencies(${EXE_NAME} ShaderConductor)

set_target_properties(${EXE_NAME} PROPERTIES FOLDER "Tools")
