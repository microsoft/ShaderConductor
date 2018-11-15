# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

set(LIB_NAME ShaderConductor)

set(SOURCE_FILES
    ${SC_ROOT_DIR}/Source/Core/ShaderConductor.cpp
)

set(HEADER_FILES
    ${SC_ROOT_DIR}/Include/ShaderConductor/ShaderConductor.hpp
)

source_group("Source Files" FILES ${SOURCE_FILES})
source_group("Header Files" FILES ${HEADER_FILES})

add_definitions(-DSHADER_CONDUCTOR_SOURCE)

include_directories(
    ${SC_ROOT_DIR}/Include
    ${SC_BUILD_DIR}/External/DirectXShaderCompiler/include
    ${SC_ROOT_DIR}/External/DirectXShaderCompiler/include
    ${SC_ROOT_DIR}/External/SPIRV-Cross
)
link_directories(
    ${SC_BUILD_DIR}/External/DirectXShaderCompiler/${CMAKE_CFG_INTDIR}/lib
    ${SC_BUILD_DIR}/External/DirectXShaderCompiler/lib/${CMAKE_CFG_INTDIR}
)

add_library(${LIB_NAME} "SHARED"
    ${SOURCE_FILES} ${HEADER_FILES}
)

target_link_libraries(${LIB_NAME}
    LLVMDxcSupport LLVMHLSL LLVMOption LLVMSupport dxcompiler SPIRV-Tools LLVMDxcSupport LLVMBitReader
    LLVMipa LLVMAnalysis LLVMCore LLVMSupport
    spirv-cross-core spirv-cross-glsl spirv-cross-hlsl spirv-cross-msl
)

if(WIN32)
    set(dxcompilerName "dxcompiler.dll")
    set(runtimeOutputFolder "bin")
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(dxcompilerName "libdxcompiler.dylib")
    set(runtimeOutputFolder "lib")
else()
    set(dxcompilerName "libdxcompiler.so")
    set(runtimeOutputFolder "lib")
endif()

set(dxcompilerOutput "${SC_BUILD_DIR}/Bin/${CMAKE_CFG_INTDIR}/${dxcompilerName}")
add_custom_command(OUTPUT ${dxcompilerOutput}
    COMMAND ${CMAKE_COMMAND} -E copy
        ${SC_BUILD_DIR}/External/DirectXShaderCompiler/${CMAKE_CFG_INTDIR}/${runtimeOutputFolder}/${dxcompilerName}
        ${SC_BUILD_DIR}/Bin/${CMAKE_CFG_INTDIR}/${dxcompilerName}
    COMMENT "Copying dxcompiler...")
add_custom_target(CopyDxcompiler DEPENDS ${dxcompilerOutput})
add_dependencies(CopyDxcompiler dxcompiler)

set_target_properties(CopyDxcompiler PROPERTIES FOLDER "Core")

add_dependencies(${LIB_NAME} spirv-cross-core spirv-cross-glsl spirv-cross-hlsl spirv-cross-msl)
add_dependencies(${LIB_NAME} CopyDxcompiler)

set_target_properties(${LIB_NAME} PROPERTIES FOLDER "Core")
