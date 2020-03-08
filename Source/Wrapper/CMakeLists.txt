# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

set(DLL_NAME ShaderConductorWrapper)

set(SOURCE_FILES
    Native.cpp
    Native.h
)

source_group("Source Files" FILES ${SOURCE_FILES})

add_library(${DLL_NAME} SHARED ${SOURCE_FILES})

target_include_directories(${DLL_NAME}
    PRIVATE
        $<TARGET_PROPERTY:ShaderConductor,INTERFACE_INCLUDE_DIRECTORIES>
)

target_link_libraries(${DLL_NAME}
    PRIVATE
        ShaderConductor
)

add_dependencies(${DLL_NAME} ShaderConductor)

set_target_properties(${DLL_NAME} PROPERTIES FOLDER "Wrapper")

set(CSHARP_TEST CSharpPinvoke)

set(SOURCE_FILES
    Program.cs
    Wrapper.cs    
)

set(DATA_FILES
    shader.hlsl 
)

set_source_files_properties(${DATA_FILES}
    PROPERTIES VS_TOOL_OVERRIDE "None"    
    		   VS_COPY_TO_OUT_DIR "PreserveNewest"
)

add_executable(${CSHARP_TEST} ${SOURCE_FILES} ${DATA_FILES})
add_dependencies(${CSHARP_TEST} ShaderConductorWrapper)

set_target_properties(${CSHARP_TEST} PROPERTIES FOLDER "Wrapper")
