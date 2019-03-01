# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

set(EXE_NAME ShaderConductorTest)

set(SOURCE_FILES
    ShaderConductorTest.cpp
)

set(DATA_INC_FILES
    Data/Input/Inc/HeaderA.hlsli
    Data/Input/Inc/HeaderB.hlsli
    Data/Input/Inc/HeaderEmpty.hlsli
)

set(DATA_FILES
    Data/Input/Common.hlsli
    Data/Input/Constant_PS.hlsl
    Data/Input/Constant_VS.hlsl
    Data/Input/DetailTessellation_HS.hlsl
    Data/Input/Fluid_CS.hlsl
    Data/Input/IncludeEmptyHeader.hlsl
    Data/Input/IncludeExist.hlsl
    Data/Input/IncludeNotExist.hlsl
    Data/Input/HalfDataType.hlsl
    Data/Input/Particle_GS.hlsl
    Data/Input/PassThrough_PS.hlsl
    Data/Input/PassThrough_VS.hlsl
    Data/Input/PNTriangles_DS.hlsl
    Data/Input/ToneMapping_PS.hlsl
    Data/Input/Transform_VS.hlsl
)

set_source_files_properties(${DATA_FILES}
    PROPERTIES VS_TOOL_OVERRIDE "None"
)

source_group("Source Files" FILES ${SOURCE_FILES})
source_group("Data Files\\Input" FILES ${DATA_FILES})
source_group("Data Files\\Input\\Inc" FILES ${DATA_INC_FILES})

link_directories(
    ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
)

add_executable(${EXE_NAME} ${SOURCE_FILES} ${DATA_FILES} ${DATA_INC_FILES})

target_include_directories(${EXE_NAME} PRIVATE
    ${SC_ROOT_DIR}/Include
    ${SC_ROOT_DIR}/External/googletest/googletest/include
)
target_compile_definitions(${EXE_NAME} PRIVATE
    -DTEST_DATA_DIR="${SC_ROOT_DIR}/Source/Tests/Data/")
target_link_libraries(${EXE_NAME}
    ShaderConductor
    debug gtestd optimized gtest
)

add_dependencies(${EXE_NAME} ShaderConductor)
add_dependencies(${EXE_NAME} gtest)

set_target_properties(${EXE_NAME} PROPERTIES FOLDER "Tests")
